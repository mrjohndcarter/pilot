# Makefile to build the Pilot documentation
#
# There are 4 kinds of documentation that can be built:
#
# 1. Man pages:  "make dist-docs" filters the Pilot functions out of pilot.h
#	and uses Doxygen to create man pages in subdir man. These can be
#	conveniently installed by typing "make install-man" in trunk. This
#	covers the C API (not the Fortran API).
#
# 2. End user Doxygen web pages:  "make dist-docs" also produces Doxygen web pages
#	in subdir html that are at the level of detail suitable for end users.
#	Start navigating at html/index.html.
#
# 3. RST documents:  These are reStructuredText files (.rst). They can be
#	converted to HTML "make flatpages". Then the HTML files can be uploaded
#	to the Pilot website via its admin > Flatpages interface.
#
# 4. Pilot developer web pages:  "make dev-docs" invokes Doxygen on the
#	entire code base in trunk (assumed to be the parent directory). This
#	output ends up in a doxy subdir, so it can coexist with "make dist-docs".

# Extract current version number from pilot_private.h #define so it can be
# passed to doxygen as the PROJECT_NUMBER
#
PI_VERSION = $(shell grep "define \+PI_VERSION" ../pilot_private.h | cut -d\" -f2)

dist-docs: Doxyfile-man
	(./doxygen_filter.py ../pilot.h > pilot.h)
	./doxygen_filter.py ../pilot_limits.h > pilot_limits.h
	(cat $<; echo "PROJECT_NUMBER = V"$(PI_VERSION)) | doxygen -
	rm pilot.h pilot_limits.h

dev-docs: Doxyfile-dev
	(cat $<; echo "PROJECT_NUMBER = V"$(PI_VERSION)) | doxygen -

flatpages: INSTALL.htm pubs.htm RELEASE_NOTES.htm tutorial.htm

# Convert an RST document to HTML, then filter out the boilerplate including
# the embedded CSS (Pilot website has its own CSS treatment)
#
%.htm: %.rst
	rst2html $< | sed -e '1,/<body>/d;/<\/body>/,$$d' > $@

clean:
	rm -rf man html doxy
	rm -f *.htm *.h
