## ADDED Requirements

### Requirement: LIMIT clause
The system SHALL support limiting the number of rows returned by a query using the LIMIT clause.

#### Scenario: Limit results to N rows
- **WHEN** user executes `SELECT * FROM users LIMIT 10`
- **THEN** at most 10 rows are returned from the result set

#### Scenario: Limit with fewer rows available
- **WHEN** user executes `SELECT * FROM users LIMIT 100` and table has 5 rows
- **THEN** all 5 rows are returned without error

#### Scenario: Limit zero
- **WHEN** user executes `SELECT * FROM users LIMIT 0`
- **THEN** zero rows are returned

### Requirement: OFFSET clause
The system SHALL support skipping rows using the OFFSET clause. OFFSET SHALL require LIMIT to be specified.

#### Scenario: Skip first N rows
- **WHEN** user executes `SELECT * FROM users LIMIT 10 OFFSET 5`
- **THEN** rows 6-15 are returned (skipping the first 5)

#### Scenario: Offset beyond available rows
- **WHEN** user executes `SELECT * FROM users LIMIT 10 OFFSET 100` and table has 50 rows
- **THEN** zero rows are returned without error

#### Scenario: Offset zero
- **WHEN** user executes `SELECT * FROM users LIMIT 10 OFFSET 0`
- **THEN** first 10 rows are returned (equivalent to no OFFSET)

### Requirement: LIMIT/OFFSET with ORDER BY
The system SHALL apply LIMIT and OFFSET after ORDER BY to enable deterministic pagination.

#### Scenario: Paginated sorted results
- **WHEN** user executes `SELECT * FROM users ORDER BY id LIMIT 10 OFFSET 20`
- **THEN** rows 21-30 are returned, sorted by `id` ascending

### Requirement: LIMIT/OFFSET validation
The system SHALL validate that LIMIT and OFFSET values are non-negative integers.

#### Scenario: Negative LIMIT
- **WHEN** user executes `SELECT * FROM users LIMIT -5`
- **THEN** system returns an error indicating LIMIT must be non-negative

#### Scenario: Negative OFFSET
- **WHEN** user executes `SELECT * FROM users LIMIT 10 OFFSET -5`
- **THEN** system returns an error indicating OFFSET must be non-negative
