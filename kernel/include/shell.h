/*
 * MicroKernel Shell Header
 * 
 * A simple kernel-level shell for debugging and interaction
 */

#ifndef _KERNEL_SHELL_H
#define _KERNEL_SHELL_H

#include "types.h"

/*
 * Initialize and run the shell
 * This function enters an infinite loop and does not return
 */
void shell_init(void);

/*
 * Run the shell (alias for shell_init)
 */
void shell_run(void);

/*
 * Process a single character input
 * Used for interrupt-driven input handling
 * 
 * @param c: The character to process
 */
void shell_input_char(char c);

#endif /* _KERNEL_SHELL_H */