## ADDED Requirements

### Requirement: ORDER BY single column sorting
The system SHALL support sorting query results by a single column in ascending or descending order. ASC SHALL be the default direction when not specified.

#### Scenario: Sort by single column ascending
- **WHEN** user executes `SELECT * FROM users ORDER BY name`
- **THEN** results are returned sorted by the `name` column in ascending order

#### Scenario: Sort by single column descending
- **WHEN** user executes `SELECT * FROM users ORDER BY age DESC`
- **THEN** results are returned sorted by the `age` column in descending order

#### Scenario: Explicit ASC keyword
- **WHEN** user executes `SELECT * FROM users ORDER BY id ASC`
- **THEN** results are returned sorted by the `id` column in ascending order

### Requirement: ORDER BY multi-column sorting
The system SHALL support sorting by multiple columns with independent sort directions. Columns SHALL be evaluated left-to-right with subsequent columns breaking ties.

#### Scenario: Multi-column with same direction
- **WHEN** user executes `SELECT * FROM users ORDER BY last_name, first_name`
- **THEN** results are sorted by `last_name` ascending, then by `first_name` ascending for ties

#### Scenario: Multi-column with mixed directions
- **WHEN** user executes `SELECT * FROM users ORDER BY department ASC, salary DESC`
- **THEN** results are sorted by `department` ascending, then by `salary` descending within each department

### Requirement: ORDER BY with qualified column names
The system SHALL support ORDER BY with table-qualified column references when table aliases are used.

#### Scenario: Order by qualified column with alias
- **WHEN** user executes `SELECT t.name FROM users t ORDER BY t.name`
- **THEN** results are sorted by the `name` column from the aliased table

### Requirement: ORDER BY validation
The system SHALL validate that ORDER BY columns exist in the result set or source table.

#### Scenario: Invalid column in ORDER BY
- **WHEN** user executes `SELECT name FROM users ORDER BY nonexistent`
- **THEN** system returns an error indicating the column does not exist
