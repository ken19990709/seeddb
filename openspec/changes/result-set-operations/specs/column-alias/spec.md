## ADDED Requirements

### Requirement: Column alias with AS keyword
The system SHALL support column aliases using the AS keyword to rename columns in the result set.

#### Scenario: Simple column alias
- **WHEN** user executes `SELECT name AS username FROM users`
- **THEN** the result column is named `username` instead of `name`

#### Scenario: Expression with alias
- **WHEN** user executes `SELECT age + 1 AS next_age FROM users`
- **THEN** the computed column is named `next_age` in the result set

#### Scenario: Multiple column aliases
- **WHEN** user executes `SELECT id AS user_id, name AS username FROM users`
- **THEN** result columns are named `user_id` and `username` respectively

### Requirement: Column alias without AS keyword
The system SHALL support column aliases without the AS keyword (implicit aliasing).

#### Scenario: Implicit alias
- **WHEN** user executes `SELECT name username FROM users`
- **THEN** the result column is named `username` (AS is optional)

### Requirement: Alias in ORDER BY
The system SHALL allow ORDER BY to reference column aliases defined in the SELECT list.

#### Scenario: Order by alias name
- **WHEN** user executes `SELECT name AS n FROM users ORDER BY n`
- **THEN** results are sorted by the `name` column using its alias `n`

### Requirement: Alias case preservation
The system SHALL preserve the case of column aliases as specified by the user.

#### Scenario: Mixed case alias
- **WHEN** user executes `SELECT name AS UserName FROM users`
- **THEN** the result column is named `UserName` preserving the case
