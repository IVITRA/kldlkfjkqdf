#!/usr/bin/env python3
"""
Source Validator - Validates credibility and quality of web sources

This module ensures that only high-quality, trustworthy sources are
used for training ASM experts. It implements multi-layered validation:

1. Domain reputation scoring
2. Content quality assessment
3. Freshness checking
4. Conspiracy/misinformation detection
5. Cross-reference validation
"""

import re
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from urllib.parse import urlparse
import json
import os


@dataclass
class ValidationResult:
    """Result of source validation"""
    url: str
    is_valid: bool
    credibility_score: float  # 0.0 to 1.0
    reasons: List[str]
    warnings: List[str]
    domain: str
    last_updated: Optional[datetime]
    content_freshness: Optional[str]  # fresh, moderate, outdated


class SourceValidator:
    """
    Multi-layered source validation system
    
    Validation layers:
    1. Domain reputation (trusted/untrusted domains)
    2. URL pattern matching (known spam patterns)
    3. Content freshness (how recent is the information)
    4. Language quality (professional vs. sensationalist)
    5. Conspiracy detection (known misinformation patterns)
    6. Cross-reference (consistency with other sources)
    """
    
    def __init__(self, config_file: str = 'validator_config.json'):
        """
        Initialize source validator
        
        Args:
            config_file: Path to validation configuration
        """
        self.config = self._load_config(config_file)
        
        # Trusted domains with credibility scores
        self.trusted_domains = {
            # Academic & Scientific
            'wikipedia.org': 0.95,
            'arxiv.org': 0.98,
            'nature.com': 0.99,
            'science.org': 0.99,
            'ncbi.nlm.nih.gov': 0.99,
            'scholar.google.com': 0.97,
            
            # Technology
            'github.com': 0.90,
            'stackoverflow.com': 0.85,
            'docs.python.org': 0.95,
            'developer.mozilla.org': 0.95,
            'microsoft.com': 0.90,
            'google.com': 0.90,
            
            # News (reputable)
            'reuters.com': 0.95,
            'apnews.com': 0.95,
            'bbc.com': 0.90,
            'nytimes.com': 0.85,
            'theguardian.com': 0.85,
            
            # Education
            'edu': 0.90,  # All .edu domains
            'coursera.org': 0.90,
            'mit.edu': 0.95,
            'stanford.edu': 0.95,
            
            # Government
            'gov': 0.90,  # All .gov domains
            'who.int': 0.98,
            'cdc.gov': 0.98,
            'nasa.gov': 0.98,
        }
        
        # Untrusted domains (blacklist)
        self.untrusted_domains = {
            'infowars.com',
            'breitbart.com',
            'naturalnews.com',
            'beforeitsnews.com',
            'reddit.com/r/conspiracy',
            '4chan.org',
            '8kun.top',
        }
        
        # Conspiracy language patterns (English)
        self.conspiracy_patterns_en = [
            r'\bthey don\'t want you to know\b',
            r'\bmainstream media (lies|coverup|cover-up)\b',
            r'\bdeep state\b',
            r'\bfalse flag\b',
            r'\bnew world order\b',
            r'\biluminati\b',
            r'\bbig pharma (hiding|covering up)',
            r'\bwoke (agenda|ideology)',
            r'\bhoax\b.*\bexposed\b',
            r'\btruth (they|will) (hide|never tell)',
            r'\bwake up (sheep|people)',
            r'\bopen your eyes\b',
            r'\b(they|government) (are|is) lying',
            r'\bsecret (knowledge|truth|agenda)',
            r'\b(coverup|cover-up|conspiracy)',
        ]
        
        # Conspiracy language patterns (Arabic)
        self.conspiracy_patterns_ar = [
            r'\bهم لا يريدونك أن تعرف\b',
            r'\bإعلام Mainstream يكذب\b',
            r'\bمؤامرة\b',
            r'\bنظرية المؤامرة\b',
            r'\bالحكومة تكذب\b',
            r'\bأكذوبة\b.*\bمكشوفة\b',
            r'\bاستيقظ\b.*\bيا\b',
            r'\bافتح عينيك\b',
            r'\bسر\b.*\bخطير\b',
            r'\bفضح\b.*\bأكذوبة\b',
        ]
        
        # Spam URL patterns
        self.spam_patterns = [
            r'\.xyz$',
            r'\.top$',
            r'\.tk$',
            r'\.ml$',
            r'\.ga$',
            r'\.cf$',
            r'bit\.ly',
            r'tinyurl\.com',
            r'\d{10,}',  # Very long numeric paths
            r'(buy|cheap|free|download).*(pills|viagra|casino|porn)',
        ]
        
        # Sensationalist language
        self.sensationalist_words = {
            'shocking', 'unbelievable', 'mind-blowing', 'you won\'t believe',
            'this will', 'goes viral', 'everyone is talking', 'breaking',
            'urgent', 'must see', 'watch before deleted',
            'صادم', 'لا يصدق', 'يجب أن ترى', 'فيروسي'
        }
        
        print("✅ Source Validator initialized")
    
    def _load_config(self, config_file: str) -> dict:
        """Load validation configuration"""
        default_config = {
            'min_credibility_score': 0.6,
            'max_content_age_days': 365,
            'check_conspiracy': True,
            'check_freshness': True,
            'allow_social_media': False,
        }
        
        if os.path.exists(config_file):
            with open(config_file, 'r') as f:
                user_config = json.load(f)
                default_config.update(user_config)
        
        return default_config
    
    def validate_url(self, url: str) -> ValidationResult:
        """
        Validate a URL for credibility and trustworthiness
        
        Args:
            url: URL to validate
        
        Returns:
            ValidationResult with credibility score and reasons
        """
        reasons = []
        warnings = []
        credibility = 0.5  # Start with neutral score
        
        # Parse URL
        parsed = urlparse(url)
        domain = parsed.netloc.lower()
        
        # Layer 1: Domain reputation
        domain_score, domain_reasons = self._check_domain_reputation(domain)
        credibility = domain_score
        reasons.extend(domain_reasons)
        
        # Layer 2: URL pattern matching
        spam_score, spam_warnings = self._check_spam_patterns(url)
        if spam_score < 0.3:
            credibility *= 0.5  # Heavy penalty for spam
            warnings.extend(spam_warnings)
        
        # Layer 3: Social media check
        if self._is_social_media(domain):
            if not self.config['allow_social_media']:
                warnings.append("Social media sources not allowed")
                credibility *= 0.7
        
        # Layer 4: Content quality (if HTML provided)
        # This would be called after scraping
        
        # Determine validity
        is_valid = (credibility >= self.config['min_credibility_score'] and
                   not any('spam' in w.lower() for w in warnings))
        
        return ValidationResult(
            url=url,
            is_valid=is_valid,
            credibility_score=credibility,
            reasons=reasons,
            warnings=warnings,
            domain=domain,
            last_updated=None,
            content_freshness=None
        )
    
    def validate_content(self, text: str, url: str, 
                        publication_date: Optional[datetime] = None) -> ValidationResult:
        """
        Validate content quality and detect misinformation
        
        Args:
            text: Content text
            url: Source URL
            publication_date: When content was published
        
        Returns:
            ValidationResult with content analysis
        """
        # Start with URL validation
        result = self.validate_url(url)
        
        # Layer 5: Conspiracy detection
        if self.config['check_conspiracy']:
            conspiracy_score, conspiracy_warnings = self._detect_conspiracy(text)
            if conspiracy_score > 0.7:
                result.is_valid = False
                result.credibility_score *= 0.3
                result.warnings.extend(conspiracy_warnings)
                result.reasons.append("High conspiracy/misinformation score")
        
        # Layer 6: Sensationalism detection
        sensationalism_score = self._detect_sensationalism(text)
        if sensationalism_score > 0.6:
            result.credibility_score *= 0.8
            result.warnings.append(f"High sensationalism score: {sensationalism_score:.2f}")
        
        # Layer 7: Content freshness
        if self.config['check_freshness'] and publication_date:
            freshness = self._check_freshness(publication_date)
            result.content_freshness = freshness
            
            if freshness == 'outdated':
                result.credibility_score *= 0.7
                result.warnings.append("Content is outdated")
        
        # Layer 8: Language quality
        language_score = self._assess_language_quality(text)
        if language_score < 0.5:
            result.credibility_score *= 0.85
            result.warnings.append("Low language quality")
        
        # Update validity
        result.is_valid = (result.credibility_score >= self.config['min_credibility_score'])
        
        return result
    
    def _check_domain_reputation(self, domain: str) -> Tuple[float, List[str]]:
        """Check domain reputation and return score with reasons"""
        reasons = []
        
        # Check exact match
        if domain in self.trusted_domains:
            score = self.trusted_domains[domain]
            reasons.append(f"Trusted domain: {domain} (score: {score})")
            return score, reasons
        
        # Check if domain is in blacklist
        if domain in self.untrusted_domains:
            reasons.append(f"Blacklisted domain: {domain}")
            return 0.1, reasons
        
        # Check TLD (.edu, .gov, etc.)
        for trusted_tld, score in self.trusted_domains.items():
            if trusted_tld.startswith('.') and domain.endswith(trusted_tld):
                reasons.append(f"Trusted TLD: {trusted_tld}")
                return score, reasons
        
        # Check subdomains of trusted sites
        for trusted_domain, score in self.trusted_domains.items():
            if domain.endswith('.' + trusted_domain):
                reasons.append(f"Subdomain of trusted: {trusted_domain}")
                return score * 0.95, reasons  # Slight reduction for subdomains
        
        # Unknown domain - neutral score
        reasons.append(f"Unknown domain: {domain}")
        return 0.5, reasons
    
    def _check_spam_patterns(self, url: str) -> Tuple[float, List[str]]:
        """Check URL against known spam patterns"""
        warnings = []
        spam_count = 0
        
        for pattern in self.spam_patterns:
            if re.search(pattern, url, re.IGNORECASE):
                spam_count += 1
                warnings.append(f"Spam pattern detected: {pattern}")
        
        if spam_count == 0:
            return 1.0, []
        elif spam_count == 1:
            return 0.5, warnings
        else:
            return 0.2, warnings
    
    def _is_social_media(self, domain: str) -> bool:
        """Check if domain is a social media platform"""
        social_media_domains = {
            'facebook.com', 'twitter.com', 'x.com', 'instagram.com',
            'tiktok.com', 'reddit.com', 'tumblr.com', 'pinterest.com',
            'snapchat.com', 'linkedin.com'
        }
        
        return domain in social_media_domains or any(
            domain.endswith('.' + sm) for sm in social_media_domains
        )
    
    def _detect_conspiracy(self, text: str) -> Tuple[float, List[str]]:
        """
        Detect conspiracy language patterns
        
        Returns:
            Tuple of (conspiracy_score, warnings)
        """
        warnings = []
        text_lower = text.lower()
        
        # Count conspiracy pattern matches
        conspiracy_matches = 0
        total_patterns = len(self.conspiracy_patterns_en) + len(self.conspiracy_patterns_ar)
        
        for pattern in self.conspiracy_patterns_en + self.conspiracy_patterns_ar:
            if re.search(pattern, text_lower, re.IGNORECASE):
                conspiracy_matches += 1
        
        if conspiracy_matches == 0:
            return 0.0, []
        
        # Calculate conspiracy score
        conspiracy_score = min(1.0, conspiracy_matches / max(1, total_patterns * 0.1))
        
        warnings.append(
            f"Conspiracy language detected: {conspiracy_matches} patterns matched"
        )
        
        return conspiracy_score, warnings
    
    def _detect_sensationalism(self, text: str) -> float:
        """
        Detect sensationalist language
        
        Returns:
            Sensationalism score (0.0 to 1.0)
        """
        text_lower = text.lower()
        words = set(text_lower.split())
        
        sensationalist_count = len(words & self.sensationalist_words)
        
        if sensationalist_count == 0:
            return 0.0
        
        # Normalize by text length
        sensationalism_score = min(1.0, sensationalist_count / 3.0)
        
        return sensationalism_score
    
    def _check_freshness(self, publication_date: datetime) -> str:
        """
        Check content freshness
        
        Returns:
            'fresh', 'moderate', or 'outdated'
        """
        age = datetime.now() - publication_date
        max_age_days = self.config['max_content_age_days']
        
        if age.days < max_age_days * 0.25:  # < 25% of max age
            return 'fresh'
        elif age.days < max_age_days * 0.75:  # < 75% of max age
            return 'moderate'
        else:
            return 'outdated'
    
    def _assess_language_quality(self, text: str) -> float:
        """
        Assess language quality (grammar, spelling, professionalism)
        
        Returns:
            Quality score (0.0 to 1.0)
        """
        if not text or len(text) < 100:
            return 0.3
        
        # Check for excessive capitalization (SHOUTING)
        uppercase_ratio = sum(1 for c in text if c.isupper()) / len(text)
        if uppercase_ratio > 0.3:
            return 0.5
        
        # Check for excessive exclamation marks!!!
        exclamation_ratio = text.count('!') / len(text)
        if exclamation_ratio > 0.05:  # More than 5% exclamation marks
            return 0.6
        
        # Check sentence length variation (good writing has variety)
        sentences = [s.strip() for s in text.split('.') if len(s.strip()) > 10]
        if len(sentences) > 5:
            avg_length = sum(len(s.split()) for s in sentences) / len(sentences)
            length_variance = sum(
                (len(s.split()) - avg_length) ** 2 
                for s in sentences
            ) / len(sentences)
            
            # Good writing has moderate variance
            if length_variance < 10:
                return 0.6
            elif length_variance < 50:
                return 0.8
            else:
                return 0.7
        
        return 0.7  # Default for short text
    
    def cross_reference(self, claims: List[str], sources: List[Dict]) -> Dict:
        """
        Cross-reference claims across multiple sources
        
        Args:
            claims: List of claims to verify
            sources: List of source dicts with 'url', 'content', 'credibility'
        
        Returns:
            Dict with claim verification results
        """
        results = {}
        
        for claim in claims:
            supporting = 0
            contradicting = 0
            source_details = []
            
            for source in sources:
                content_lower = source.get('content', '').lower()
                claim_lower = claim.lower()
                
                # Simple keyword matching (in production, use NLI model)
                claim_words = set(claim_lower.split())
                content_words = set(content_lower.split())
                
                overlap = len(claim_words & content_words) / len(claim_words)
                
                if overlap > 0.5:
                    supporting += 1
                    source_details.append({
                        'url': source['url'],
                        'supports': True,
                        'overlap': overlap,
                        'credibility': source.get('credibility', 0.5)
                    })
                elif overlap > 0.2 and any(
                    neg_word in content_lower 
                    for neg_word in ['not', 'never', 'false', 'incorrect', 'debunked']
                ):
                    contradicting += 1
                    source_details.append({
                        'url': source['url'],
                        'supports': False,
                        'overlap': overlap,
                        'credibility': source.get('credibility', 0.5)
                    })
            
            # Determine verification status
            if supporting > contradicting:
                status = 'supported'
                confidence = supporting / (supporting + contradicting + 1)
            elif contradicting > supporting:
                status = 'contradicted'
                confidence = contradicting / (supporting + contradicting + 1)
            else:
                status = 'inconclusive'
                confidence = 0.5
            
            results[claim] = {
                'status': status,
                'confidence': confidence,
                'supporting_sources': supporting,
                'contradicting_sources': contradicting,
                'sources': source_details
            }
        
        return results
    
    def batch_validate(self, urls: List[str]) -> List[ValidationResult]:
        """
        Validate multiple URLs at once
        
        Args:
            urls: List of URLs to validate
        
        Returns:
            List of validation results
        """
        results = []
        
        for url in urls:
            result = self.validate_url(url)
            results.append(result)
        
        # Sort by credibility (highest first)
        results.sort(key=lambda r: r.credibility_score, reverse=True)
        
        return results
    
    def get_trusted_sources_for_domain(self, domain: str) -> List[str]:
        """
        Get list of trusted sources for a specific domain
        
        Args:
            domain: Knowledge domain (medical, legal, etc.)
        
        Returns:
            List of trusted URLs
        """
        domain_sources = {
            'medical': [
                'https://www.ncbi.nlm.nih.gov',
                'https://www.who.int',
                'https://www.cdc.gov',
                'https://www.nejm.org',
            ],
            'legal': [
                'https://www.law.cornell.edu',
                'https://www.supremecourt.gov',
                'https://www.justia.com',
            ],
            'science': [
                'https://www.nature.com',
                'https://www.science.org',
                'https://arxiv.org',
            ],
            'technology': [
                'https://docs.microsoft.com',
                'https://developer.mozilla.org',
                'https://stackoverflow.com',
            ],
        }
        
        return domain_sources.get(domain, [])


if __name__ == "__main__":
    print("="*60)
    print("Source Validator - Test Mode")
    print("="*60)
    
    validator = SourceValidator()
    
    # Test URLs
    test_urls = [
        "https://www.nature.com/articles/s41586-023-06002-4",
        "https://en.wikipedia.org/wiki/Machine_learning",
        "https://arxiv.org/abs/2301.12345",
        "https://infowars.com/secret-conspiracy",
        "https://suspicious-site.xyz/free-viagra",
    ]
    
    print("\n🔍 Validating URLs:\n")
    
    for url in test_urls:
        result = validator.validate_url(url)
        
        status = "✅ VALID" if result.is_valid else "❌ INVALID"
        print(f"{status} - {url}")
        print(f"   Credibility: {result.credibility_score:.2f}")
        print(f"   Reasons: {', '.join(result.reasons[:2])}")
        if result.warnings:
            print(f"   ⚠️  Warnings: {', '.join(result.warnings[:2])}")
        print()
    
    # Test content validation
    print("\n📝 Testing content validation:\n")
    
    sample_content = """
    New research from MIT demonstrates that machine learning models
    can achieve 95% accuracy on image classification tasks. The study,
    published in Nature, involved training transformer models on
    ImageNet dataset with novel attention mechanisms.
    """
    
    result = validator.validate_content(
        text=sample_content,
        url="https://www.nature.com/articles/example",
        publication_date=datetime.now() - timedelta(days=30)
    )
    
    print(f"Content Validation Result:")
    print(f"   Valid: {result.is_valid}")
    print(f"   Credibility: {result.credibility_score:.2f}")
    print(f"   Freshness: {result.content_freshness}")
    print(f"   Warnings: {result.warnings}")
    
    print("\n✅ Test complete!")
