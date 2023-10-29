### how to publish

- Tag the commit with a version string like this one `v0.2.2` (not anything else i.e. like this `v.0.2.2`) we need a full (not lightweight) tag. Use i.e.

 ```git tag v0.2.3 -m "v0.2.3"```

- To verify you can use `git show-ref --tags -d --abbrev=7` keep an eye on the `^{}`. Only full tags have such a suffix where the starting hash points to the commit having the tag. Therefore, there are at least 2 lines for each tag.
```
55bdf63 refs/tags/v1.0.6
bc2b94d refs/tags/v1.0.6^{}
37deb88 refs/tags/v1.0.7
fe0d08b refs/tags/v1.0.7^{}
a6ac105 refs/tags/v1.0.8
933891e refs/tags/v1.0.8^{}
215c5e4 refs/tags/v1.0.9
e37c12e refs/tags/v1.0.9^{}
```

- Tags are local, they must be pushed to remote too
    - push all tags `git push --follow-tags`
    - push a single one `git push origin v0.2.3`
    - lightweight tags only ??`git push --tags`
    - or equivalent via GUI (whatever this way do...)
    
- Or even better we use versiontag (modified! to versiontag.sh) from https://github.com/franiglesias/versiontag
    - `./versiontag.sh current` -> show current version
    -  `./versiontag.sh patch` -> patch
    -  `./versiontag.sh minor` -> minor
    -  `./versiontag.sh major` -> major
    
- created publish.sh script to check if everything is committed before doing versioning and build

- On the github action runner `github describe` works, but the `-dirty` flag doesn't. The `--abbrev=0` flag doesn't work either.
