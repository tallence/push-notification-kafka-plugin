#!/bin/sh -e

package_name="push-notification-kafka-plugin"

if [ ! -d .git ]; then
    echo "no .git present. run this from the base dir of the git checkout."
    exit 1
fi

version=$1
[ -z "$version" ] && version=`git describe --match 'v*' | sed 's/^v//'`
outfile="$package_name-$version"

echo "version $version"

# clean out old cruft...
echo "cleanup..."
rm -f $outfile.tar $outfile.tar.gz

# build new tarball
echo "building tarball..."
bin/git-archive-all.sh --prefix $outfile/ \
		       --verbose \
		       $outfile.tar
echo "compressing..."
xz -z9 $outfile.tar

echo "done."
