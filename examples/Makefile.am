bin_PROGRAMS=connect detect tracks files hotplug \
	folders trexist playlists getplaylist \
	format albumart albums newplaylist emptyfolders \
	thumb reset

connect_SOURCES=connect.c delfile.c getfile.c newfolder.c \
	sendfile.c sendtr.c pathutils.c pathutils.h \
	util.c util.h common.h
detect_SOURCES=detect.c util.c util.h common.h
tracks_SOURCES=tracks.c util.c util.h common.h
files_SOURCES=files.c util.c util.h common.h
hotplug_SOURCES=hotplug.c util.c util.h common.h
folders_SOURCES=folders.c util.c util.h common.h
trexist_SOURCES=trexist.c util.c util.h common.h
playlists_SOURCES=playlists.c util.c util.h common.h
getplaylist_SOURCES=getplaylist.c util.c util.h common.h
newplaylist_SOURCES=newplaylist.c util.c util.h common.h
format_SOURCES=format.c util.c util.h common.h
albumart_SOURCES=albumart.c util.c util.h common.h
albums_SOURCES=albums.c util.c util.h common.h
emptyfolders_SOURCES=emptyfolders.c util.c util.h common.h
thumb_SOURCES=thumb.c util.c util.h common.h
reset_SOURCES=reset.c util.c util.h common.h

AM_CPPFLAGS=-I$(top_builddir)/src
LDADD=../src/libmtp.la
EXTRA_DIST=evolution-sync.sh

install-exec-hook:
	ln -f -s $(DESTDIR)$(bindir)/mtp-connect$(EXEEXT) $(DESTDIR)$(bindir)/mtp-delfile$(EXEEXT)
	ln -f -s $(DESTDIR)$(bindir)/mtp-connect$(EXEEXT) $(DESTDIR)$(bindir)/mtp-getfile$(EXEEXT)
	ln -f -s $(DESTDIR)$(bindir)/mtp-connect$(EXEEXT) $(DESTDIR)$(bindir)/mtp-newfolder$(EXEEXT)
	ln -f -s $(DESTDIR)$(bindir)/mtp-connect$(EXEEXT) $(DESTDIR)$(bindir)/mtp-sendfile$(EXEEXT)
	ln -f -s $(DESTDIR)$(bindir)/mtp-connect$(EXEEXT) $(DESTDIR)$(bindir)/mtp-sendtr$(EXEEXT)
