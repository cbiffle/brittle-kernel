#ifndef K_APP_H
#define K_APP_H

namespace k {

/*
 * Interprets the AppInfo block to create kernel objects, prepares the initial
 * Context, and hands control to unprivileged code.  This is intended to be the
 * last action of main.
 */
__attribute__((noreturn))
void start_app();

}  // namespace k

#endif  // K_APP_H
