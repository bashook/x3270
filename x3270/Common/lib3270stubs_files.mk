# Source files for lib3270stubs.
LIB3270STUBS_SOURCES = menubar_stubs.c popups_stubs.c screen_stubs1.c \
	screen_stubs2.c screen_stubs3.c ssl_passwd_gui_stubs.c

# Object files for lib3270stubs.
LIB3270STUBS_OBJECTS = menubar_stubs.$(OBJ) popups_stubs.$(OBJ) \
	screen_stubs1.$(OBJ) screen_stubs2.$(OBJ) screen_stubs3.$(OBJ) \
	ssl_passwd_gui_stubs.$(OBJ)

# Header files for lib3270stubs.
LIB3270STUBS_HEADERS = conf.h globals.h localdefs.h menubar.h popupsc.h
