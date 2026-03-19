## ADDED Requirements

### Requirement: Table alias with AS keyword
The system SHALL support table aliases using the AS keyword in the FROM clause.

#### Scenario: Table alias with AS
- **WHEN** user executes `SELECT t.name FROM users AS t`
- **THEN** the query succeeds using `t` as an alias for `users`

### Requirement: Table alias without AS keyword
The system SHALL support table aliases without the AS keyword (implicit aliasing).

#### Scenario: Implicit table alias
- **WHEN** user executes `SELECT t.name FROM users t`
- **THEN** the query succeeds using `t` as an alias for `users`

### Requirement: Qualified column references with alias
The system SHALL support column references qualified with the table alias.

#### Scenario: Select with qualified columns
- **WHEN** user executes `SELECT t.id, t.name FROM users t`
- **THEN** columns are resolved from the aliased table

### Requirement: Alias required for qualified access
When a table alias is defined, column references MAY use either the alias or the original table name.

#### Scenario: Reference by original name with alias defined
- **WHEN** user executes `SELECT users.name FROM users t`
- **THEN** the query succeeds (original table name still valid)

#### Scenario: Reference by alias
- **WHEN** user executes `SELECT t.name FROM users t`
- **THEN** the query succeeds using the alias

### Requirement: Alias in WHERE clause
The system SHALL support table alias references in the WHERE clause.

#### Scenario: Alias in WHERE condition
- **WHEN** user executes `SELECT * FROM users t WHERE t.age > 21`
- **THEN** the condition is evaluated using the aliased table reference

### Requirement: Alias case sensitivity
Table aliases SHALL be case-insensitive for reference purposes but case-preserving for display.

#### Scenario: Case-insensitive alias reference
- **WHEN** user executes `SELECT T.name FROM users t`
- **THEN** the query succeeds (T matches t case-insensitively)
