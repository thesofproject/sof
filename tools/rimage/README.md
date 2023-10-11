# rimage

`rimage` is a DSP firmware image creation and signing tool targeting
the DSP on certain Intel System-on-Chip (SoC). This is used by
the [Sound Open Firmware (SOF)](https://github.com/thesofproject/sof)
to generate binary image files.

## Building

Most SOF users never build `rimage` directly but as an ExternalProject
defined by CMake in SOF. This makes sure they always use an up-to-date
version of rimage and configuration files that have been fully tested.

If needed, `rimage` can be built manually with the usual CMake commands:

```shell
$ cmake -B build/
$ make  -C build/ help # lists all targets
$ make  -C build/
```

The `build/rimage` executable can then be copied to a directory in the
PATH. Zephyr users can run `west config rimage.path
/path/to/rimage/build/rimage`; Zephyr documentation and `west sign -h`
have more details.

## Testing rimage changes with SOF Continuous Integration

This section is about leveraging SOF validation to test rimage changes
_before_ submitting them to the rimage repository.

Nothing here is actually specific to SOF and rimage; you can apply the
same test logic to any submodule and parent on Github. In fact the same
logic applies to submodule alternatives. Github is the only requirement.

### Get familiar with git submodules

This is unfortunately not optional for SOF and rimage.

For various reasons submodules seem to confuse many git users. Maybe
because the versions of the submodules are not directly visible in some
configuration file like with most alternatives? Either way, an
unfortunate prerequisite before doing any rimage work is to get familiar
with git submodules in general. As submodules are built-in there are
many resources about them on the Internet. One possible starting point
is https://git-scm.com/book/en/v2/Git-Tools-Submodules but feel free
to use any other good tutorial instead. Make sure you actually practice
a tutorial; don't just read it. Practicing on a temporary and throw-away
copy of SOF + rimage is a great idea.

Obviously, you also need to be familiar with regular Github pull
requests.

### Run SOF tests on unmerged rimage commits

First, push the rimage commits you want to be tested to any branch of
your rimage fork on Github.  Do _not_ submit an rimage pull request yet.

Note your rimage fork must have been created using the actual "fork"
button on Github so Github is aware of the connection with the upstream
rimage repo. In the top-left corner you should see `forked from
thesofproject/rimage` under the name of your fork. If not then search
the Internet for "re-attach detached github fork".

Then, **pretend** these rimage commits have already been accepted and
merged (they have been neither) and submit to SOF a draft pull request
that updates the main SOF branch with your brand new rimage commits to
test. The only SOF commit in this SOF TEST pull request is an SOF commit
that updates the rimage pointer to the SHA of your last rimage
commit. If you're not sure how to do this then you must go back to the
previous section and practice submodules more.

Submit this SOF pull request as a Github _draft_ so reviewers are _not_
notified. Starting every pull request as a draft is always a good idea
but in this case this particular SOF pull request can be especially
confusing because it points at commits in a different repo and commits
that are not merged yet. So you _really_ don't want to bother busy
reviewers (here's a secret: some of the reviewers don't like submodules
either). You can freely switch back and forth between draft and ready
status and should indeed switch to draft if you forgot at submission
time but you can never "un-notify" reviewers.

Github has very good support for submodules and will display your SOF
TEST pull request better than what the git command line can show. For
instance Github will list your rimage changes directly in the SOF Pull
Request. So if something looks unexpected on Github then it means you
did something wrong. Stop immediately (except for switching to draft if
you forgot) and ask the closest git guru for help.

Search for "Submodule" in the build logs and make sure the last of your
new rimage commits has been checked out.

Iterate and force-push your rimage branch and your SOF TEST pull request
until all the SOF tests pass. Then you can submit your rimage pull
request as usual. In the comments section of the rimage pull request,
point at your test results on the SOF side to impress the rimage
reviewers and get your rimage changes merged faster.

Finally, after your rimage changes have been merged, you can if you want
submit one final SOF pull request that points to the final rimage
SHA. Or, if your rimage change is not urgently needed, you can just wait
for someone else to do it later. If you do it, copy the rimage git log
--oneline in the SOF commit message. Find some good (and less good)
rimage commit message examples at
https://github.com/thesofproject/sof/commits/main/rimage
