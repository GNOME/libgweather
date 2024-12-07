Contributing to libgweather
===========================

Thank you for considering contributing to libgweather!

These guidelines are meant for new contributors, regardless of their level
of proficiency; following them allows the maintainers of libgweather to more
effectively evaluate your contribution, and provide prompt feedback to you.
Additionally, by following these guidelines you clearly communicate that you
respect the time and effort that the people developing libgweather put into
managing the project.

This project is free software, and it would not exist without contributions
from the free and open source software community. There are many things that
we value:

 - bug reporting and fixing
 - documentation and examples
 - tests
 - testing and support for other platforms
 - new features

Please, do **not** use the issue tracker for support questions. If you have
questions on how to use libgweather effectively, you can use:

 - the [`gweather` tag on GNOME's Discourse](https://discourse.gnome.org/tags/gweather)

The issue tracker is meant to be used for actionable issues only.

## How to report bugs

### Security issues

You should not open a new issue for security related questions.

Always use the [security report form](https://security.gnome.org) to ensure
the relevant people are notified of your issue.

### Bug reports

If you’re reporting a bug make sure to list:

 0. which version of libgweather are you using?
 0. which operating system are you using?
 0. the necessary steps to reproduce the issue
 0. the expected outcome
 0. a description of the behavior
 0. a small, self-contained example exhibiting the behavior

If the issue includes a crash, you should also include:

 0. the eventual warnings printed on the terminal
 0. a backtrace, obtained with tools such as GDB or LLDB

If the issue includes a memory leak, you should also include:

 0. a log of definite leaks from a tool such as [valgrind’s memcheck](http://valgrind.org/docs/manual/mc-manual.html)

For small issues, such as:

 - spelling/grammar fixes in the documentation,
 - typo correction,
 - comment clean ups,
 - changes to metadata files (CI, `.gitignore`),
 - build system changes, or
 - source tree clean ups and reorganizations;

or for self-contained bug fixes where you have implemented and tested a solution
already, you should directly open a merge request instead of filing a new issue.

### Features and enhancements

Feature discussion can be open ended and require high bandwidth channels; if
you are proposing a new feature on the issue tracker, make sure to make
an actionable proposal, and list:

 0. what you’re trying to achieve and the problem it solves
 0. three (or more) existing pieces of software which would benefit from the
    new feature

New APIs, in particular, should follow the ‘rule of three’, where there should
be three (or more) pieces of software which are ready to use the new APIs. This
allows us to check that the new APIs are usable in real-life code, and fit well
with related APIs.

If proposing a large feature or change, it’s better to discuss it on
[Discourse](https://discourse.gnome.org) before putting time into writing an
actionable issue — and certainly before putting time into writing a merge
request.

## Your first contribution

### Prerequisites

If you want to contribute to the libgweather project, you will need to have
the development tools appropriate for your operating system, including:

 - Python 3.x
 - Meson
 - Ninja
 - Gettext (19.7 or newer)
 - a [C99 compatible compiler](https://wiki.gnome.org/Projects/GLib/CompilerRequirements)

Up-to-date instructions about developing GNOME applications and libraries
can be found on [the GNOME Developer Center](https://developer.gnome.org).

The [libgweather project uses GitLab](https://gitlab.gnome.org/GNOME/libgweather/)
for code hosting and for tracking issues. More information about using GitLab
can be found [on the GNOME wiki](https://wiki.gnome.org/GitLab).

### Getting started

You should start by forking the libgweather repository from the GitLab web
UI, and cloning from your fork:

```sh
$ git clone https://gitlab.gnome.org/yourusername/libgweather.git
$ cd libgweather
```

To compile the Git version of libgweather on your system, you will need to
configure your build using Meson:

```sh
$ meson setup _builddir .
$ meson compile -C _builddir
```

Typically, you should work on your own branch:

```sh
$ git switch -C your-branch
```

The coding style of libgweather is maintained through
[clang-format](https://clang.llvm.org/docs/ClangFormat.html). The
configuration is provided by libgweather itself. Before committing your
changes, you should run:

```sh
clang-format \
	--style=file \
	libgweather/*.c \
	libgweather/tests/*.c \
	libgweather/tools/*.c
```

to ensure that the changes are formatted using the same coding style as the
rest of the project. The project's own continuous integration pipeline will
enforce the coding style.

Once you’ve finished working on the bug fix or feature, push the branch
to the Git repository and open a new merge request, to let the libgweather
core developers review your contribution.

### Code reviews

Each contribution is reviewed by the core developers of the libgweather
project.

With each code review, we intend to:

 0. Identify if this is a desirable change or new feature. Ideally for larger
    features this will have been discussed (in an issue, on IRC, or on
    Discourse) already, so that effort isn’t wasted on putting together merge
    requests which will be rejected.
 0. Check the design of any new API.
 0. Provide realistic estimates of how long a review might take, if it can’t
    happen immediately.
 0. Ensure that all significant contributions of new code, or bug fixes, are
    adequately tested, either through requiring tests to be submitted at the
    same time, or as a follow-up.
 0. Ensure that all new APIs are documented and have
    [introspection annotations](https://gi.readthedocs.org).
 0. Check that the contribution is split into logically separate commits, each
    with a good commit message.
 0. Encourage further high quality contributions.
 0. Ensure code style and quality is upheld.

If a code review is stalled (due to not receiving comments for two or more
weeks; or due to a technical disagreement), please ping the libgweather
maintainers on the merge request, or on IRC, to ask for a second opinion.

### Commit messages

The expected format for git commit messages is as follows:

```plain
Short explanation of the commit

Longer explanation explaining exactly what’s changed, whether any
external or private interfaces changed, what bugs were fixed (with bug
tracker reference if applicable) and so forth. Be concise but not too
brief.

Closes #1234
```

 - Always add a brief description of the commit to the _first_ line of
 the commit and terminate by two newlines (it will work without the
 second newline, but that is not nice for the interfaces).

 - First line (the brief description) must only be one sentence and
 should start with a capital letter unless it starts with a lowercase
 symbol or identifier. Don’t use a trailing period either. Don’t exceed
 72 characters.

 - The main description (the body) is normal prose and should use normal
 punctuation and capital letters where appropriate. Consider the commit
 message as an email sent to the developers (or yourself, six months
 down the line) detailing **why** you changed something. There’s no need
 to specify the **how**: the changes can be inlined.

 - When committing code on behalf of others use the `--author` option, e.g.
 `git commit -a --author "Joe Coder <joe@coder.org>"` and `--signoff`.

 - If your commit is addressing an issue, use the
 [GitLab syntax](https://docs.gitlab.com/ce/user/project/issues/managing_issues.html#default-closing-pattern)
 to automatically close the issue when merging the commit with the upstream
 repository:

```plain
Closes #1234
Fixes #1234
Closes: https://gitlab.gnome.org/GNOME/libgweather/issues/1234
```

 - If you have a merge request with multiple commits and none of them
 completely fixes an issue, you should add a reference to the issue in
 the commit message, e.g. `Bug: #1234`, and use the automatic issue
 closing syntax in the description of the merge request.

### Notes for developers in the GNOME group on GitLab

Don't commit any but the most trivial patches without approval.

Exceptions to the rule above:

 - Translators may commit basic i18n related patches to the build setup.
 - Release team members are welcome to commit merge requests to unbreak
   the GNOME build.

## Adding new weather sources

To add new weather sources, a number of requirements must be considered:

- the API should be documented
- access to the API can require the use of a freely available application
  token ID. See the requirements below.
- there must not be hard usage limits, or they must be high enough not to
  cause disruption with weather support being builtin to the OS/desktop
- the requests must be made over HTTPS, or an equivalent, encrypted network
  connection.
- the result of requests must be machine parseable, and the parsing code
  must have unit tests to prevent regressions, and make it easy to root-cause
  crashes
- the user’s privacy must be maintained as much as possible, see section below
- the user’s bandwidth must be preserved where possible, with the server offering
  enough information to avoid downloads through libsoup, our HTTP library.
- the data gathered can require a attribution (in which case the patch must
  contain code to implement this) and if restricted to non-commercial usage,
  must have a comment mentioning that fact in the GWeatherWeather API documentation
- finally, a working and applicable patch must be provided

## Privacy

The user’s privacy must be maintained as much as possible:

- don’t include unnecessary detail in requests
- don’t allow fine-grained location tracking
- don’t include other identifying information in the requests if possible
- service must have a data usage policy that's reasonable compared to
  equivalent services, eg. not use requests as a way to feed into user tracking
  for an advertisment business

## Application token usage

Not using tokens is preferable, but some data sources don't offer the option.
There are a number of requirements for those tokens:

- One should be provided in the patch for testing purposes, and be easily
  overridable by distributions wishing to have a separate identifier and limits
- The test token should have high enough limits that you're reasonably confident
  that lots of people running `meson test` won’t cause the token to be revoked
  and break everyone’s tests
- Instructions on how to get a token for the application must be provided
