import os

package = 'libt3key'
srcdirs = [ 'src', 'src.util' ]
excludesrc = "/(Makefile|TODO.*|SciTE.*|run(\.sh)?|debug|test\.c)$"
auxsources= [ 'src.util/t3keyc/.objects/*.c', 'src/.objects/map.bytes', 'src/key_api.h', 'src/key_errors.h', 'src/key_shared.c' ]
extrabuilddirs = [ 'doc' ]
auxfiles = [ 'doc/API', 'doc/format.txt', 'doc/supplemental.kmap' ]

versioninfo = '1:0:0'

def get_replacements(mkdist):
	return [
		{
			'tag': '<VERSION>',
			'replacement': mkdist.version
		},
		{
			'tag': '^#define T3_KEY_VERSION .*',
			'replacement': '#define T3_KEY_VERSION ' + mkdist.get_version_bin(),
			'files': [ 'src/key.h' ],
			'regex': True
		},
		{
			'tag': '<OBJECTS>',
			'replacement': " ".join(mkdist.sources_to_objects(mkdist.include_by_regex(mkdist.sources, '^src/'), '\.c$', '.lo')),
			'files': [ 'mk/libt3key.in' ]
		},
		{
			'tag': '<OBJECTS>',
			'replacement': " ".join(mkdist.sources_to_objects(mkdist.include_by_regex(mkdist.sources, '^src\.util/t3keyc/'), '\.c$', '.o')),
			'files': [ 'mk/t3keyc.in' ]
		},
		{
			'tag': '<OBJECTS>',
			'replacement': " ".join(mkdist.sources_to_objects(mkdist.include_by_regex(mkdist.sources, '^src\.util/t3learnkeys/'), '\.c$', '.o')),
			'files': [ 'mk/t3learnkeys.in' ]
		},
		{
			'tag': '<VERSIONINFO>',
			'replacement': versioninfo,
			'files': [ 'mk/libt3key.in' ]
		},
		{
			'tag': '<LIBVERSION>',
			'replacement': versioninfo.split(':', 2)[0],
			'files': [ 'Makefile.in', 'mk/*.in' ]
		}
	]

def finalize(mkdist):
	os.symlink('.', os.path.join(mkdist.topdir, 'src', 't3key'))
	print("WARNING: need to add validation of the descriptions to the mkdist script!!!")
