man_MANS = majacd.1

SUBDIRS = src \
		  @GUI_DIR@

#mgiscdocdir = ${prefix}/doc/exp
#mgiscdoc_DATA = \
#	README\
#	COPYING\
#	AUTHORS\
#	ChangeLog\
#	INSTALL\
#	NEWS\
#	TODO

EXTRA_DIST = majacd.spec.in majacd.1

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

RPMDIR = ${PWD}
rpm: dist-bzip2
	rpmbuild \
		--define "_sourcedir $(RPMDIR)" \
		--define "_specdir $(RPMDIR)" \
		--define "_builddir $(RPMDIR)" \
		--define "_srcrpmdir $(RPMDIR)" \
		--define "_rpmdir $(RPMDIR)" \
		-ba majacd.spec
