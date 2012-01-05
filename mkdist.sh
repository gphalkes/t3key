#!/bin/bash


cd "`dirname \"$0\"`"
BASEDIR="$PWD"

. ../../repo-scripts/mkdist_funcs.sh

setup_hg
get_version_hg
check_mod_hg
build_all
[ -z "${NOBUILD}" ] && { make -C doc clean ; make -C doc all ; }
get_sources_hg
make_tmpdir
copy_sources ${SOURCES} ${GENSOURCES} ${AUXSOURCES}
copy_dist_files
copy_files doc/API doc/format.txt `hg manifest | egrep '^man/'`
create_configure

if [[ "${VERSION}" =~ [0-9]{8} ]] ; then
	VERSION_BIN=0
else
	VERSION_BIN="$(printf "0x%02x%02x%02x" $(echo ${VERSION} | tr '.' ' '))"
fi

sed -i "s/<VERSION>/${VERSION}/g" `find ${TOPDIR} -type f`
sed -i "/#define T3_KEY_VERSION/c #define T3_KEY_VERSION ${VERSION_BIN}" ${TOPDIR}/src/key.h

( cd ${TOPDIR}/src ; ln -s . t3key )

OBJECTS_LIBT3KEY="`echo \"${SOURCES} ${GENSOURCES} ${AUXSOURCES}\" | tr ' ' '\n' | sed -r 's%\.objects/%%' | egrep '^src/[^/]*\.c$' | sed -r 's/\.c\>/.lo/g' | tr '\n' ' '`"
OBJECTS_T3KEYC="`echo \"${SOURCES} ${GENSOURCES} ${AUXSOURCES}\" | tr ' ' '\n' | sed -r 's%\.objects/%%' | egrep '^src\.util/t3keyc/.*\.c$' | sed -r 's/\.c\>/.o/g' | tr '\n' ' '`"
OBJECTS_T3LEARNKEYS="`echo \"${SOURCES} ${GENSOURCES} ${AUXSOURCES}\" | tr ' ' '\n' | sed -r 's%\.objects/%%' | egrep '^src\.util/t3learnkeys/.*\.c$' | sed -r 's/\.c\>/.o/g' | tr '\n' ' '`"

#FIXME: somehow verify binary compatibility, and print an error if not compatible
LIBVERSION="${VERSIONINFO%%:*}"

sed -r -i "s%<LIBVERSION>%${LIBVERSION}%g" ${TOPDIR}/Makefile.in ${TOPDIR}/mk/libt3key.in
sed -r -i "s%<OBJECTS_LIBT3KEY>%${OBJECTS_LIBT3KEY}%g;\
s%<VERSIONINFO>%${VERSIONINFO}%g" ${TOPDIR}/mk/libt3key.in
sed -r -i "s%<OBJECTS_T3KEYC>%${OBJECTS_T3KEYC}%g" ${TOPDIR}/mk/t3keyc.in
sed -r -i "s%<OBJECTS_T3LEARNKEYS>%${OBJECTS_T3LEARNKEYS}%g" ${TOPDIR}/mk/t3learnkeys.in

# Modify parser output to look for files in current directory iso .objects
sed -r -i 's%\.objects/%%g' ${TOPDIR}/src.util/t3keyc/grammar.c

echo "WARNING: need to add validation of the descriptions to the mkdist script!!!"
create_tar
