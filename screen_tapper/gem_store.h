// Move: the EEPROM slot/index logic, checksums, lifetime/session update rules.

// In .ino: replace direct EEPROM references with clean calls like “get lifetime”, “add to session”, and “flush if needed”.

// Compile/test: print the totals and confirm no regression.