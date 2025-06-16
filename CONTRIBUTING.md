# Contributing to Artemis PDU Firmware

## Code Style and Documentation Guidelines

### 1. Code Documentation
- **Function Documentation**: Every function must have a clear header comment explaining:
  - Purpose of the function
  - Input parameters
  - Return values
  - Any side effects
  ```c
  /**
   * @brief Brief description of what the function does
   * @param param1 Description of first parameter
   * @param param2 Description of second parameter
   * @return Description of return value
   */
  ```

- **Code Comments**: 
  - Add inline comments for complex logic
  - Explain "why" not just "what" the code does
  - Keep comments up-to-date with code changes

### 2. Code Organization
- Keep functions small and focused (max 50 lines)
- Use meaningful variable and function names
- Group related functions together
- Follow the existing project structure

### 3. Error Handling
- Always check return values from function calls
- Provide meaningful error messages
- Document error conditions in function headers

### 4. Version Control
- Write clear, descriptive commit messages
- One logical change per commit
- Reference issue numbers in commit messages

### 5. Testing
- Document test cases
- Include test results in documentation
- Test edge cases and error conditions

## Project Structure
```
src/
├── core/           # Core functionality
├── drivers/        # Hardware drivers
├── utils/          # Utility functions
└── tests/          # Test files
```

## Best Practices
1. **Keep It Simple**
   - Write straightforward, readable code
   - Avoid premature optimization
   - Use standard library functions when possible

2. **Documentation First**
   - Update documentation before making code changes
   - Include diagrams for complex systems
   - Document hardware interfaces

3. **Code Review Process**
   - Self-review before submitting
   - Check for:
     - Documentation completeness
     - Error handling
     - Code style compliance
     - Test coverage

4. **Hardware Considerations**
   - Document hardware dependencies
   - Include pin configurations
   - Note timing requirements

## Getting Started
1. Read the README.md
2. Review existing code and documentation
3. Set up development environment
4. Run existing tests
5. Start with small, well-documented changes

## Questions and Support
- Document questions and solutions
- Update documentation with new findings
- Share knowledge with team members

Remember: The goal is to maintain code that is:
- Easy to understand
- Well-documented
- Maintainable by future students
- Reliable and safe 