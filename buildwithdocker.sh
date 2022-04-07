# -e exit if any command fails, -x print commands as they are executed
set -ex
# make output dir
mkdir -p build/arm64

# set correct version
GIT_COMMIT=`git rev-parse --short HEAD`
GIT_TAG=`git describe --abbrev=0 --dirty`
VERSION_STRING="$GIT_TAG ($GIT_COMMIT)"

# build container
docker buildx build --no-cache --progress plain -t "ogi-it/sdmon:latest" --platform linux/arm64 --build-arg VERSION_STRING=${VERSION_STRING} ./

# create container
docker create --name sdmonbuild ogi-it/sdmon

# copy
docker cp sdmonbuild:/usr/src/app/sdmon ./build/arm64
base64 ./build/arm64/sdmon > ./build/arm64/sdmon.txt

#cleanup
docker container stop sdmonbuild
docker container rm sdmonbuild
docker image rm ogi-it/sdmon:latest