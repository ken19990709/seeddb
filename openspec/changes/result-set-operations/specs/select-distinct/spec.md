## ADDED Requirements

### Requirement: SELECT DISTINCT single column
The system SHALL support DISTINCT to eliminate duplicate rows when selecting a single column.

#### Scenario: Distinct single column
- **WHEN** user executes `SELECT DISTINCT department FROM employees`
- **THEN** each unique department value appears exactly once in the result

### Requirement: SELECT DISTINCT multiple columns
The system SHALL support DISTINCT across multiple columns, eliminating rows where ALL selected columns match.

#### Scenario: Distinct multiple columns
- **WHEN** user executes `SELECT DISTINCT department, role FROM employees`
- **THEN** each unique combination of (department, role) appears exactly once

### Requirement: SELECT DISTINCT with all columns
The system SHALL support DISTINCT with SELECT * to eliminate fully duplicate rows.

#### Scenario: Distinct all columns
- **WHEN** user executes `SELECT DISTINCT * FROM employees` and table has duplicate rows
- **THEN** duplicate rows are eliminated from the result

### Requirement: DISTINCT with ORDER BY
The system SHALL support combining DISTINCT with ORDER BY. DISTINCT SHALL be applied before sorting.

#### Scenario: Distinct with sorting
- **WHEN** user executes `SELECT DISTINCT department FROM employees ORDER BY department`
- **THEN** unique departments are returned in sorted order

### Requirement: DISTINCT NULL handling
The system SHALL treat multiple NULL values as duplicates in DISTINCT evaluation.

#### Scenario: Distinct with NULLs
- **WHEN** user executes `SELECT DISTINCT manager_id FROM employees` and multiple rows have NULL manager_id
- **THEN** only one NULL value appears in the result

### Requirement: DISTINCT position
The DISTINCT keyword SHALL appear immediately after SELECT.

#### Scenario: Invalid DISTINCT position
- **WHEN** user executes `SELECT name DISTINCT FROM users`
- **THEN** system returns a syntax error
