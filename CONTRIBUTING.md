# Contributing to OrderFlow

Thank you for considering contributing to OrderFlow. This document outlines the process and guidelines for contributions.

## Getting Started

1. Fork the repository
2. Clone your fork locally
3. Create a feature branch from master
4. Make your changes
5. Submit a pull request

## Development Setup

### Prerequisites

- C++17 compatible compiler
- CMake 3.16 or higher
- Git

### Building

```bash
mkdir build && cd build
cmake ..
make
```

### Testing

Run the benchmark to verify performance has not regressed:

```bash
./build/benchmark
```

Expected results should be comparable to documented performance metrics.

## Code Guidelines

### Style

- Use consistent indentation (4 spaces)
- Follow existing naming conventions
- Keep functions focused and single-purpose
- Add comments for complex logic

### Performance

- Maintain O(log n) or better complexity for core operations
- Avoid unnecessary allocations
- Profile before and after changes that affect hot paths
- Run benchmarks to verify no performance regression

### Correctness

- Ensure price-time priority matching is preserved
- Verify partial fills work correctly
- Test edge cases (empty book, single order, large quantities)
- Check for memory leaks with valgrind

## Submitting Changes

### Pull Request Process

1. Update documentation if adding features or changing APIs
2. Ensure code compiles without warnings
3. Run existing executables to verify functionality
4. Describe changes clearly in the pull request description
5. Reference any related issues

### Commit Messages

Use clear, descriptive commit messages:

```
Add support for iceberg orders

- Implement hidden quantity tracking
- Update matching logic to reveal quantity incrementally
- Add tests for iceberg order behavior
```

## Feature Requests

Open an issue describing:

- The use case or problem
- Proposed solution
- Any performance implications
- Alternative approaches considered

## Bug Reports

Include:

- Steps to reproduce
- Expected behavior
- Actual behavior
- Environment (compiler, OS, CMake version)
- Minimal code example if applicable

## Areas for Contribution

### High Priority

- Unit test framework and comprehensive tests
- Thread-safe variant with lock-free data structures
- Additional order types (stop orders, iceberg orders)
- Network interface for remote order submission
- Market data visualization tools

### Documentation

- Architecture diagrams
- Integration examples for different use cases
- Performance tuning guide
- API reference expansion

### Performance

- SIMD optimizations
- Cache-aware data structure layouts
- Alternative data structures (skip lists, B+ trees)
- Profiling and benchmarking improvements

## Questions

For questions about contributing, open a discussion issue or contact the maintainers.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
