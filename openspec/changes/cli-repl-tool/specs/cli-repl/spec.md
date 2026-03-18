## ADDED Requirements

### Requirement: REPL Loop Execution

The CLI SHALL provide an interactive Read-Eval-Print Loop that continuously accepts SQL input, executes it, and displays results until the user exits.

#### Scenario: Basic REPL session
- **WHEN** user starts seeddb-cli
- **THEN** system displays welcome message and prompt "seeddb> "

#### Scenario: Execute single SQL statement
- **WHEN** user types "SELECT 1;" and presses Enter
- **THEN** system executes the query and displays the result

#### Scenario: Exit REPL
- **WHEN** user types "\q" or presses Ctrl+D
- **THEN** system exits gracefully with "Bye." message

### Requirement: Multi-line SQL Input

The CLI SHALL support multi-line SQL statements, accumulating input until a semicolon terminator is encountered.

#### Scenario: Multi-line statement entry
- **WHEN** user types "SELECT" and presses Enter without semicolon
- **THEN** system displays continuation prompt "     -> " and waits for more input

#### Scenario: Complete multi-line statement
- **WHEN** user completes a statement with "FROM users;" on a new line
- **THEN** system executes the complete accumulated SQL statement

### Requirement: Table-Formatted Result Display

The CLI SHALL display SELECT query results in a formatted ASCII table with column headers, borders, and row count.

#### Scenario: Display query results
- **WHEN** query returns rows with columns (id, name)
- **THEN** system displays:
  ```
  +------+-------+
  | id   | name  |
  +------+-------+
  | 1    | Alice |
  +------+-------+
  (1 row)
  ```

#### Scenario: Display empty result
- **WHEN** query returns zero rows
- **THEN** system displays table header and "(0 rows)"

#### Scenario: Column width auto-sizing
- **WHEN** column values have varying lengths
- **THEN** column width adjusts to fit the longest value or header name

### Requirement: DDL Command Feedback

The CLI SHALL display confirmation messages for DDL commands (CREATE TABLE, DROP TABLE).

#### Scenario: CREATE TABLE success
- **WHEN** user executes "CREATE TABLE users (id INTEGER, name VARCHAR(50));"
- **THEN** system displays "CREATE TABLE"

#### Scenario: DROP TABLE success
- **WHEN** user executes "DROP TABLE users;"
- **THEN** system displays "DROP TABLE"

### Requirement: DML Command Feedback

The CLI SHALL display affected row counts for DML commands (INSERT, UPDATE, DELETE).

#### Scenario: INSERT success
- **WHEN** user executes "INSERT INTO users VALUES (1, 'Alice');"
- **THEN** system displays "INSERT 0 1"

#### Scenario: UPDATE success
- **WHEN** user executes "UPDATE users SET name = 'Bob' WHERE id = 1;"
- **THEN** system displays "UPDATE 1" (affected row count)

#### Scenario: DELETE success
- **WHEN** user executes "DELETE FROM users WHERE id = 1;"
- **THEN** system displays "DELETE 1" (affected row count)

### Requirement: Syntax Error Display

The CLI SHALL display syntax errors with line/column information and visual pointer to the error location.

#### Scenario: Display syntax error
- **WHEN** user executes "SELECT * FORM users;"
- **THEN** system displays:
  ```
  ERROR:  syntax error at or near "FORM"
  LINE 1: SELECT * FORM users;
                   ^
  ```

### Requirement: Execution Error Display

The CLI SHALL display execution errors with clear error messages.

#### Scenario: Table not found error
- **WHEN** user executes "SELECT * FROM nonexistent;"
- **THEN** system displays "ERROR: relation \"nonexistent\" does not exist"

#### Scenario: Column not found error
- **WHEN** user executes "SELECT bad_col FROM users;"
- **THEN** system displays "ERROR: column \"bad_col\" does not exist"

### Requirement: Meta Commands

The CLI SHALL support backslash meta commands for non-SQL operations.

#### Scenario: Quit command
- **WHEN** user types "\q"
- **THEN** system exits the REPL

#### Scenario: Help command
- **WHEN** user types "\?" or "\h"
- **THEN** system displays available commands

#### Scenario: List tables command
- **WHEN** user types "\dt"
- **THEN** system displays list of all tables in the database

#### Scenario: Unknown meta command
- **WHEN** user types "\unknown"
- **THEN** system displays "Invalid command \\unknown. Try \\? for help."

### Requirement: NULL Value Display

The CLI SHALL display NULL values distinctly from empty strings.

#### Scenario: Display NULL value
- **WHEN** query result contains a NULL value
- **THEN** system displays "NULL" (or configurable placeholder) in the cell
