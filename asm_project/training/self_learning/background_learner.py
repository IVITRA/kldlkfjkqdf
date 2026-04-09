#!/usr/bin/env python3
"""
Background Learner - 24/7 continuous learning daemon for ASM

This module runs continuous learning sessions to improve ASM experts
by processing failed queries, validating new knowledge, and triggering
expert updates automatically.

Schedule:
- Hourly: Process failed queries from the day
- Daily (3 AM): Deep learning session with knowledge consolidation
- Weekly: Full expert retraining and router rebuild
"""

import schedule
import time
import threading
import json
import os
from datetime import datetime, timedelta
from pathlib import Path
from typing import List, Dict, Optional
import logging

from knowledge_ingestor import KnowledgeIngestor, TrainingSample
from source_validator import SourceValidator

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('background_learner.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger("BackgroundLearner")


class BackgroundLearner:
    """
    Autonomous learning daemon that continuously improves ASM
    
    Responsibilities:
    1. Monitor failed queries and search for answers
    2. Validate and ingest new knowledge
    3. Trigger expert training when sufficient data collected
    4. Maintain learning schedule (hourly, daily, weekly)
    5. Consolidate knowledge to prevent contradictions
    """
    
    def __init__(self, config_file: str = 'learner_config.json'):
        """
        Initialize background learner
        
        Args:
            config_file: Path to configuration file
        """
        self.config = self._load_config(config_file)
        self.validator = SourceValidator()
        self.ingestor = KnowledgeIngestor()
        
        self.failed_queries_file = self.config.get('failed_queries_file', 'failed_queries.jsonl')
        self.knowledge_base_file = self.config.get('knowledge_base_file', 'knowledge_base.json')
        self.training_data_dir = self.config.get('training_data_dir', 'training_samples')
        
        # Ensure directories exist
        Path(self.training_data_dir).mkdir(parents=True, exist_ok=True)
        
        # Load existing knowledge base
        self.knowledge_base = self._load_knowledge_base()
        
        logger.info("✅ Background Learner initialized")
        logger.info(f"   Failed queries file: {self.failed_queries_file}")
        logger.info(f"   Knowledge base: {len(self.knowledge_base)} entries")
    
    def _load_config(self, config_file: str) -> dict:
        """Load configuration from file or use defaults"""
        default_config = {
            'hourly_session_max_queries': 50,
            'daily_session_max_sources': 100,
            'weekly_consolidation_threshold': 1000,
            'min_confidence_for_training': 0.7,
            'max_knowledge_age_days': 30,
            'auto_train_enabled': True,
            'failed_queries_file': 'failed_queries.jsonl',
            'knowledge_base_file': 'knowledge_base.json',
            'training_data_dir': 'training_samples',
            'daily_session_time': '03:00',  # 3 AM
            'weekly_consolidation_day': 'sunday'
        }
        
        if os.path.exists(config_file):
            with open(config_file, 'r') as f:
                user_config = json.load(f)
                default_config.update(user_config)
        
        return default_config
    
    def _load_knowledge_base(self) -> Dict:
        """Load existing knowledge base"""
        if os.path.exists(self.knowledge_base_file):
            with open(self.knowledge_base_file, 'r') as f:
                return json.load(f)
        return {'entries': [], 'metadata': {'last_updated': None, 'total_sources': 0}}
    
    def start(self):
        """Start the background learning daemon"""
        logger.info("🚀 Starting Background Learner daemon...")
        
        # Schedule hourly sessions
        schedule.every().hour.do(self.hourly_learning_session)
        
        # Schedule daily deep learning (3 AM)
        schedule.every().day.at(self.config['daily_session_time']).do(
            self.daily_deep_learning
        )
        
        # Schedule weekly consolidation (Sunday)
        getattr(schedule.every(), self.config['weekly_consolidation_day']).do(
            self.weekly_consolidation
        )
        
        logger.info("📅 Learning schedule configured:")
        logger.info(f"   - Hourly sessions: Every hour")
        logger.info(f"   - Daily deep learning: {self.config['daily_session_time']}")
        logger.info(f"   - Weekly consolidation: {self.config['weekly_consolidation_day'].title()}")
        
        # Run in background thread
        learning_thread = threading.Thread(target=self._run_scheduler, daemon=True)
        learning_thread.start()
        
        logger.info("✅ Background Learner running in background")
        
        return learning_thread
    
    def _run_scheduler(self):
        """Run the scheduler loop"""
        while True:
            schedule.run_pending()
            time.sleep(60)  # Check every minute
    
    def hourly_learning_session(self):
        """
        Hourly session: Process recent failed queries
        
        This session:
        1. Reads failed queries from the last hour
        2. Searches for answers online
        3. Validates and ingests new knowledge
        4. Updates knowledge base
        """
        logger.info("\n" + "="*60)
        logger.info("⏰ Starting Hourly Learning Session")
        logger.info("="*60)
        
        try:
            # Read failed queries from last hour
            failed_queries = self._read_recent_failed_queries(hours=1)
            
            if not failed_queries:
                logger.info("✅ No failed queries in last hour. Skipping.")
                return
            
            logger.info(f"📊 Found {len(failed_queries)} failed queries")
            
            # Process queries
            processed = 0
            for query in failed_queries[:self.config['hourly_session_max_queries']]:
                success = self._process_failed_query(query)
                if success:
                    processed += 1
            
            logger.info(f"✅ Hourly session complete: {processed}/{len(failed_queries)} queries processed")
            
            # Log statistics
            self._log_session_stats('hourly', processed, len(failed_queries))
            
        except Exception as e:
            logger.error(f"❌ Hourly session failed: {e}", exc_info=True)
    
    def daily_deep_learning(self):
        """
        Daily deep learning session (3 AM)
        
        This session:
        1. Processes all failed queries from the day
        2. Performs comprehensive web searches
        3. Ingests multiple sources per query
        4. Validates knowledge consistency
        5. Prepares training data for expert updates
        """
        logger.info("\n" + "="*60)
        logger.info("🌙 Starting Daily Deep Learning Session")
        logger.info("="*60)
        
        try:
            # Read all failed queries from last 24 hours
            failed_queries = self._read_recent_failed_queries(hours=24)
            
            if not failed_queries:
                logger.info("✅ No failed queries in last 24 hours. Skipping.")
                return
            
            logger.info(f"📊 Found {len(failed_queries)} failed queries in last 24h")
            
            # Process each query with multiple sources
            new_samples = []
            processed = 0
            
            for query in failed_queries[:self.config['daily_session_max_sources']]:
                # Try multiple sources
                samples = self._process_query_deep(query, max_sources=3)
                if samples:
                    new_samples.extend(samples)
                    processed += 1
            
            # Save training samples
            if new_samples:
                timestamp = datetime.now().strftime('%Y%m%d')
                output_file = os.path.join(
                    self.training_data_dir, 
                    f'samples_{timestamp}.json'
                )
                self.ingestor.save_samples(new_samples, output_file)
                
                logger.info(f"✅ Saved {len(new_samples)} training samples")
            
            logger.info(f"✅ Daily session complete: {processed}/{len(failed_queries)} queries")
            
            # Log statistics
            self._log_session_stats('daily', processed, len(failed_queries), 
                                   len(new_samples))
            
            # Trigger auto-training if enough samples collected
            if self.config['auto_train_enabled'] and len(new_samples) > 100:
                logger.info("🎯 Sufficient samples collected. Triggering expert training...")
                self._trigger_expert_training(new_samples)
            
        except Exception as e:
            logger.error(f"❌ Daily session failed: {e}", exc_info=True)
    
    def weekly_consolidation(self):
        """
        Weekly knowledge consolidation
        
        This session:
        1. Reviews entire knowledge base
        2. Removes outdated information (> 30 days)
        3. Detects and resolves contradictions
        4. Rebuilds training datasets
        5. Triggers full expert retraining if needed
        """
        logger.info("\n" + "="*60)
        logger.info("📚 Starting Weekly Knowledge Consolidation")
        logger.info("="*60)
        
        try:
            # 1. Remove outdated entries
            old_entries = self._remove_outdated_entries()
            logger.info(f"🗑️  Removed {old_entries} outdated entries")
            
            # 2. Detect contradictions
            contradictions = self._detect_contradictions()
            logger.info(f"⚠️  Found {len(contradictions)} potential contradictions")
            
            # 3. Resolve contradictions (keep most recent/higher confidence)
            resolved = self._resolve_contradictions(contradictions)
            logger.info(f"✅ Resolved {resolved} contradictions")
            
            # 4. Consolidate by domain
            domain_stats = self._consolidate_by_domain()
            logger.info("📊 Domain statistics:")
            for domain, count in domain_stats.items():
                logger.info(f"   {domain}: {count} samples")
            
            # 5. Trigger full retraining if threshold reached
            total_samples = sum(domain_stats.values())
            if total_samples > self.config['weekly_consolidation_threshold']:
                logger.info(f"🎯 {total_samples} samples collected. Triggering full retraining...")
                self._trigger_full_retraining(domain_stats)
            
            # Save consolidated knowledge base
            self._save_knowledge_base()
            
            logger.info("✅ Weekly consolidation complete")
            
        except Exception as e:
            logger.error(f"❌ Weekly consolidation failed: {e}", exc_info=True)
    
    def _read_recent_failed_queries(self, hours: int = 1) -> List[Dict]:
        """Read failed queries from the last N hours"""
        queries = []
        
        if not os.path.exists(self.failed_queries_file):
            return queries
        
        cutoff_time = datetime.now() - timedelta(hours=hours)
        
        with open(self.failed_queries_file, 'r') as f:
            for line in f:
                try:
                    query = json.loads(line)
                    query_time = datetime.fromisoformat(query.get('timestamp', ''))
                    
                    if query_time >= cutoff_time:
                        queries.append(query)
                except (json.JSONDecodeError, ValueError):
                    continue
        
        return queries
    
    def _process_failed_query(self, query: Dict) -> bool:
        """
        Process a single failed query
        
        Returns:
            True if successfully processed
        """
        try:
            question = query.get('question', '')
            domain = query.get('domain', 'general')
            
            if not question:
                return False
            
            # Search for answer (placeholder - integrate with web agent)
            logger.info(f"🔍 Searching for: {question[:50]}...")
            
            # In production, this would:
            # 1. Use CuriousSearchAgent to find sources
            # 2. Scrape content with AdaptiveScraper
            # 3. Validate with SourceValidator
            # 4. Ingest with KnowledgeIngestor
            
            # For now, simulate successful processing
            return True
            
        except Exception as e:
            logger.error(f"Failed to process query: {e}")
            return False
    
    def _process_query_deep(self, query: Dict, max_sources: int = 3) -> List[TrainingSample]:
        """
        Process query with multiple sources for deep learning
        
        Returns:
            List of training samples generated
        """
        try:
            question = query.get('question', '')
            
            if not question:
                return []
            
            logger.info(f"🔍 Deep processing: {question[:50]}...")
            
            # In production:
            # 1. Search multiple sources (3+ engines)
            # 2. Scrape and extract content
            # 3. Validate each source
            # 4. Ingest high-quality content
            # 5. Cross-reference answers
            
            # Placeholder: return empty list
            return []
            
        except Exception as e:
            logger.error(f"Deep processing failed: {e}")
            return []
    
    def _remove_outdated_entries(self) -> int:
        """Remove knowledge entries older than threshold"""
        cutoff_date = datetime.now() - timedelta(
            days=self.config['max_knowledge_age_days']
        )
        
        original_count = len(self.knowledge_base['entries'])
        
        self.knowledge_base['entries'] = [
            entry for entry in self.knowledge_base['entries']
            if datetime.fromisoformat(entry['timestamp']) >= cutoff_date
        ]
        
        return original_count - len(self.knowledge_base['entries'])
    
    def _detect_contradictions(self) -> List[Dict]:
        """Detect contradictory entries in knowledge base"""
        contradictions = []
        
        # Group by similar questions
        question_groups = {}
        for entry in self.knowledge_base['entries']:
            # Simple grouping by first 50 characters
            key = entry['question'][:50]
            if key not in question_groups:
                question_groups[key] = []
            question_groups[key].append(entry)
        
        # Check for different answers to similar questions
        for key, entries in question_groups.items():
            if len(entries) > 1:
                answers = set(entry['answer'] for entry in entries)
                if len(answers) > 1:
                    contradictions.append({
                        'question_pattern': key,
                        'entries': entries,
                        'different_answers': len(answers)
                    })
        
        return contradictions
    
    def _resolve_contradictions(self, contradictions: List[Dict]) -> int:
        """Resolve contradictions by keeping most recent/confident"""
        resolved = 0
        
        for contradiction in contradictions:
            entries = contradiction['entries']
            
            # Sort by timestamp (newest first) and confidence
            sorted_entries = sorted(
                entries,
                key=lambda x: (x.get('confidence', 0), x['timestamp']),
                reverse=True
            )
            
            # Keep the best entry, mark others as resolved
            best_entry = sorted_entries[0]
            for entry in sorted_entries[1:]:
                entry['status'] = 'resolved'
                entry['replaced_by'] = best_entry['sample_id']
                resolved += 1
        
        return resolved
    
    def _consolidate_by_domain(self) -> Dict[str, int]:
        """Consolidate knowledge base by domain"""
        domain_counts = {}
        
        for entry in self.knowledge_base['entries']:
            if entry.get('status') == 'resolved':
                continue
            
            domain = entry.get('domain', 'general')
            domain_counts[domain] = domain_counts.get(domain, 0) + 1
        
        return domain_counts
    
    def _trigger_expert_training(self, samples: List[TrainingSample]):
        """Trigger training for affected experts"""
        try:
            # Group samples by domain
            domain_samples = {}
            for sample in samples:
                domain = sample.domain
                if domain not in domain_samples:
                    domain_samples[domain] = []
                domain_samples[domain].append(sample)
            
            # Trigger training for each domain
            for domain, domain_sample_list in domain_samples.items():
                logger.info(f"🎯 Training {domain} expert with {len(domain_sample_list)} samples")
                
                # In production, call training pipeline:
                # subprocess.run([
                #     'python', 'training/directory_trainer.py',
                #     '--domain', domain,
                #     '--samples', f'training_samples/{domain}.json'
                # ])
            
        except Exception as e:
            logger.error(f"Expert training trigger failed: {e}")
    
    def _trigger_full_retraining(self, domain_stats: Dict[str, int]):
        """Trigger full expert retraining across all domains"""
        logger.info("🚀 Triggering full expert retraining...")
        
        # In production:
        # 1. Export all training data
        # 2. Run directory trainer for all domains
        # 3. Rebuild HNSW router with new experts
        # 4. Validate new experts
        # 5. Deploy updated experts
        
        logger.info("✅ Full retraining triggered (implementation needed)")
    
    def _log_session_stats(self, session_type: str, processed: int, 
                          total: int, samples_generated: int = 0):
        """Log session statistics"""
        logger.info(f"\n📊 {session_type.title()} Session Statistics:")
        logger.info(f"   Queries processed: {processed}/{total}")
        logger.info(f"   Success rate: {(processed/total*100) if total > 0 else 0:.1f}%")
        logger.info(f"   Samples generated: {samples_generated}")
        logger.info(f"   Knowledge base size: {len(self.knowledge_base['entries'])}")
    
    def _save_knowledge_base(self):
        """Save knowledge base to file"""
        self.knowledge_base['metadata']['last_updated'] = datetime.now().isoformat()
        
        with open(self.knowledge_base_file, 'w') as f:
            json.dump(self.knowledge_base, f, indent=2)
        
        logger.info(f"💾 Knowledge base saved ({len(self.knowledge_base['entries'])} entries)")
    
    def add_failed_query(self, question: str, domain: str = 'general', 
                        context: Optional[Dict] = None):
        """
        Add a failed query to the processing queue
        
        Args:
            question: The question that failed
            domain: Domain classification
            context: Additional context (user intent, etc.)
        """
        entry = {
            'question': question,
            'domain': domain,
            'timestamp': datetime.now().isoformat(),
            'context': context or {},
            'status': 'pending'
        }
        
        with open(self.failed_queries_file, 'a') as f:
            f.write(json.dumps(entry) + '\n')
    
    def stop(self):
        """Stop the background learner"""
        logger.info("🛑 Stopping Background Learner...")
        schedule.clear()
        self._save_knowledge_base()
        logger.info("✅ Background Learner stopped")


if __name__ == "__main__":
    print("="*60)
    print("ASM Background Learner - 24/7 Continuous Learning")
    print("="*60)
    
    learner = BackgroundLearner()
    
    # Start the daemon
    thread = learner.start()
    
    print("\n✅ Background Learner is running...")
    print("📋 Scheduled tasks:")
    print(f"   - Hourly learning: Every hour")
    print(f"   - Daily deep learning: 3:00 AM")
    print(f"   - Weekly consolidation: Sunday")
    print("\n💡 Press Ctrl+C to stop")
    
    try:
        # Keep main thread alive
        while True:
            time.sleep(60)
    except KeyboardInterrupt:
        learner.stop()
        print("\n👋 Background Learner stopped")
