/*
 * CoreServices is a stub umbrella framework (see CoreServices.h). The Makefile
 * lists CoreServices.c in SRCS but the file was missing from the tree, so the
 * framework dylib had nothing to compile. Provide an empty translation unit,
 * matching the stub convention used by sibling frameworks (e.g. OpenGL.c).
 */
void CoreServices(void);

void CoreServices(void) {
}
