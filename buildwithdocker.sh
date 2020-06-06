# -e exit if any command fails, -x print commands as they are executed
set -ex
# make output dir
mkdir -p build/arm64

# build container
docker buildx build --no-cache --progress plain -t "ogi-it/sdmon:latest" --platform linux/arm64 ./

# create container
docker create --name sdmonbuild ogi-it/sdmon

# copy
docker cp sdmonbuild:/usr/src/app/sdmon ./build/arm64

#cleanup
docker container stop sdmonbuild
docker container rm sdmonbuild
docker image rm ogi-it/sdmon:latest