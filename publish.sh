if git diff-index --quiet HEAD
then
  echo "clean, calling ./versiontag.sh $1"
  ./versiontag.sh $1
else
  echo "There are uncommitted changes! You must commit first."
fi