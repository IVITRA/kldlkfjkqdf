# Quick Start Guide - ASM Core Engine

## 🚀 After Building

### 1. Run the Comprehensive Stress Test (RECOMMENDED FIRST!)

This runs ALL critical checks to verify your system is ready:

```bash
cd build\Release
asm_core.exe --stress-test
```

**Expected Output:**
```
======================================================================
         COMPREHENSIVE STRESS TEST (Torture Test)
======================================================================

[Test 1] Dequantization Speed Test
  Testing ternary decompression for 1000 experts...
  Total time for 1000 experts: 5ms
  Average per expert: 0.005ms
  ✓ PASSED (< 50ms for 1000 experts)

[Test 2] LRU Cache Eviction Test
  Loading 50 experts (cache limit: 20)...
  ✓ PASSED

[Test 3] Cross-Domain Context Bridge Test
  Physics → Philosophy translation
  ✓ PASSED (Context preserved across domains)

[Test 4] Router Performance (1000 experts)
  Avg search latency: 0.234ms
  ✓ PASSED (< 0.5ms target)

[Test 5] Memory Pool Pre-allocation Test
  ✓ PASSED (Zero malloc during runtime)

[Test 6] Expert File Size Verification
  Total expected: 1MB
  ✓ PASSED (Within 2MB target)

[Test 7] System RAM Verification
  Total RAM: 8 GB
  ✓ PASSED (≥ 4GB)

======================================================================
✅ ALL CRITICAL TESTS PASSED!
   System is ready for Intel i3 (2015) deployment.
======================================================================
```

---

### 2. Run Basic Unit Tests

```bash
asm_core.exe --test
```

Runs Google Test suite for all components.

---

### 3. Run Performance Benchmarks

```bash
asm_core.exe --benchmark
```

**Expected Output:**
```
=== Running Benchmarks ===

[Router Benchmark]
  Graph construction (1000 experts): 45ms
  Search latency (avg over 100 searches): 234μs ✓

[Memory Manager Benchmark]
  Hot cache size: 0
  RAM usage: 0 bytes
  Registered experts: 0

=== Benchmarks Complete ===
```

---

### 4. View Telemetry & Diagnostics

```bash
asm_core.exe --diagnose
```

Shows detailed performance metrics and system health.

---

### 5. Interactive Mode (Default)

```bash
asm_core.exe
```

Chat with the system (demo mode - full inference requires trained experts).

---

## 📋 All Commands

| Command | Description |
|---------|-------------|
| `asm_core.exe` | Interactive mode (default) |
| `asm_core.exe --test` | Run unit tests |
| `asm_core.exe --benchmark` | Run performance benchmarks |
| `asm_core.exe --stress-test` | **Run comprehensive stress test** ⭐ |
| `asm_core.exe --diagnose` | Show telemetry report |
| `asm_core.exe --help` | Show help message |

---

## ✅ What the Stress Test Checks

1. **Dequantization Speed** - Can decompress 1000 experts in < 50ms?
2. **LRU Cache** - Does it properly evict old experts?
3. **Context Bridge** - Can it translate between domains?
4. **Router Performance** - Is search < 0.5ms for 1000 experts?
5. **Memory Pool** - Is there zero malloc at runtime?
6. **File Sizes** - Are experts ~2MB each?
7. **System RAM** - Does system have ≥ 4GB?

---

## 🎯 Success Criteria

Your system is **READY** if you see:
```
✅ ALL CRITICAL TESTS PASSED!
   System is ready for Intel i3 (2015) deployment.
```

If any test fails, check:
- **Dequantization too slow**: Ensure AVX2 is enabled in CMake
- **Router too slow**: Reduce number of experts or increase ef_search
- **RAM < 4GB**: Enable ultra-compact mode (automatic)
- **Expert files too large**: Check quantization in Python training

---

## 🔧 Troubleshooting

### Build Failed
- Install Visual Studio 2022 with "Desktop development with C++"
- Or install MinGW-w64 and add to PATH

### Tests Fail
- Run `asm_core.exe --stress-test` to see detailed diagnostics
- Check `CRITICAL_CHECKS.md` for expected values

### Slow Performance
- Ensure CPU supports AVX2 (Intel Haswell 2013+)
- Run in Release mode (not Debug)
- Check telemetry with `--diagnose`

---

## 📊 Performance Targets

| Metric | Target | Command to Check |
|--------|--------|------------------|
| Router Latency | < 0.5ms | `--stress-test` |
| Expert Load | < 50ms | `--diagnose` |
| Cache Hit Rate | > 80% | `--diagnose` |
| Expert Size | ~2MB | `--stress-test` |
| RAM Usage | < 2GB | `--diagnose` |

---

## 🎓 Next Steps

1. ✅ Build the project
2. ✅ Run `--stress-test` (must pass!)
3. ✅ Run `--benchmark` (check performance)
4. ✅ Train experts with Python pipeline
5. ✅ Deploy on target hardware

---

**Need Help?** Check `CRITICAL_CHECKS.md` for detailed explanations.
