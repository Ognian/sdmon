#!/usr/bin/env bash

set -e

# Initializes flags and other variables

action=''           # the sub-command to execute
dry=false           # Flag to run with executing commands
force=false         # Flag to run without asking for confirmation
versionFile=false   # Flag to generate the .semver file
last=''             # Will contain the last version tag found

## Runs a command with arguments, taking care of the --dry flag
function execute {
    cmd=("$@")
    if [[ true == "$dry" ]];then
        printf '%s \n' "DRY -no changes- ${cmd[*]}"
        return
    fi

    printf '%s\n' "Executing ${cmd[*]} ..."

    "${cmd[@]}" 2>&1
}

# Asks for confirmation returns yes|no
function confirm() {
    read -p "$1 ([y]es or [N]o): "
    case $(echo $REPLY | tr '[A-Z]' '[a-z]') in
        y|yes) echo "yes" ;;
        *)     echo "no" ;;
    esac
}

# Generates the .semver file

function updateSemverFile() {
    if [[ true == "$dry" || false == "$versionFile" ]]; then
        return 0
    fi

    :> .semver
    exec 3<> .semver

    echo "---" >&3
    echo ":major: $major" >&3
    echo ":minor: $minor" >&3
    echo ":patch: $patch" >&3
    echo >&3

    echo ":notes: $msg" >&3
    echo >&3
    exec 3>&-
}

function pushSemver() {
    execute git add .semver
    execute git commit -m "Update .semver file"
    execute git push
}

# Removes last tag both local and remote

function removeTag() {
    execute git tag -d "$last"
    execute git push origin :"$last"
    updateSemverFile
}

# Shows full help for the script

function showHelp() {
cat <<'TXT'
versiontag
==========

Automates version tag creation.

Commands:   patch|minor|major 'Tag message'
            remove
            current

Options:    -d|--dry:     don't change things
            -f|--force:   don't ask for confirmation
            -h|--help:    show this help
            -m|--message: annotate tag with a message
            -s|--semver:  generate .semver file

Examples:
---------

Show current version:  versiontag current
Patch version:         versiontag patch 'Fix broken view'
Minor version:         versiontag minor --message'Add Customer filter by email'
Major version:         versiontag major -m 'Blog module'
Remove last version:   versiontag remove
Run without changes:   versiontag --dry remove

TXT
}

# Shows the result of the command

function showSuccess() {
    echo
    echo "Tag v$major.$minor.$patch was created in local repository."
    echo
    echo "Push it:"
    echo
    echo "    git push origin v$major.$minor.$patch"
    echo
    echo "Delete tag:"
    echo
    echo "    git tag -d v$major.$minor.$patch"
    echo
}

# Show current version

function showCurrentVersion() {
    if [[ true == "$versionFile" ]]; then
        updateSemverFile
        printf '.semver file generated for: %s\n' "$last"
        return
    fi
    printf 'Current version: %s\n' "$last"
}

# Get the last tag and breaks into parts

function getLastTag() {
    # lasttag=`git tag -l --sort=-committerdate | head -1 2> /dev/null` || true
    # ogi: changed to suite my needs (on OSX reverse comitterdate DOESN'T WORK, and we have to ignore the latest tag too)
    lasttag=`git tag -l 'v*' | xargs -I@ git log --format=format:"%ai @%n" -1 @|sort -r| awk '{print $4}'| head -1 2> /dev/null` || true

    if [[ -z "$lasttag" ]]; then
        lasttag='v0.0.0'
    fi

    lasttag=${lasttag:1}

    parts=(${lasttag//./ })

    major=${parts[0]}
    minor=${parts[1]}
    patch=${parts[2]}

    last="v$major.$minor.$patch"
}

# Get last tag from repo and process the desired command

getLastTag

# Process command options

for ((i = 1; i <= $#; i++ )); do
    case ${!i} in
        "-d"|"--dry")
            dry=true
            ;;
        "-f"|"--force")
            force=true
            ;;
        "-h"|"--help")
            showHelp
            exit 0
            ;;
        "-m"|"--message")
            let i++
            msg="${!i}"
            ;;
        "-s"|"--semver")
            versionFile=true
            ;;
        "patch"|"minor"|"major")
            action="${!i}"
            ;;
        "current")
            action='current'
            ;;
        "remove")
            if [[ "yes" == $(confirm "Do you want to remove $last?") ]]; then
                removeTag
            fi
            exit 0
            ;;
        "help")
            showHelp
            exit 0
            ;;
        *)
            echo "${!i} option is not valid"
            exit 1
            ;;
    esac
done

case "$action" in
    'current')
        showCurrentVersion
        exit 0;;
    'patch')
        patch=$((patch + 1))
        ;;
    'minor')
        minor=$((minor + 1))
        patch=0
        ;;
    'major')
        major=$((major + 1))
        minor=0
        patch=0
        ;;
    *)
        echo "No mode selected"
        exit 0
esac

# Shows information

printf "Current version : %s\n" "$last"
printf "New tag         : %s.%s.%s\n\n" "v$major" "$minor" "$patch"

if [[ false == "$force" &&  "no" == $(confirm "Do you agree?") ]]; then
    printf '\n%s' "No tag was created"
    exit 0;
fi

if [[ -z $msg ]]; then
    # execute git tag "v$major.$minor.$patch"
    # ogi: changed to suite my needs: ALWAYS make an annotated tag!!
    execute git tag -a "v$major.$minor.$patch" -m "releasing v$major.$minor.$patch"
else
    printf 'Message         : %s\n\n' "$msg"
    execute git tag -a "v$major.$minor.$patch" -m "$msg ($action version)"
fi

updateSemverFile

if [[ false == "$force" &&  "no" == $(confirm "Do you want to push this tag right now?") ]]; then
    printf '\n%s' "No tag was pushed"
    exit 0;
fi

execute git push origin "v$major.$minor.$patch"

exit 0