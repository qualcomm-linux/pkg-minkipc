# pkg-minkipc

This is the debian packaging side of the minkipc project.
Upstream project is hosted here : https://github.com/qualcomm/minkipc

## Branches

**main**: Contains the code-of-conduct, contributing, licence, etc. and dispatch_workflows in .github folder.
          This branch does not contain any upstream code or debian/ metadata

**debian/latest** : Branch is categorized as per DEP-14 specification: https://dep-team.pages.debian.net/deps/dep14/

**upstream/latest** : Branch is categorized as per DEP-14 specification: https://dep-team.pages.debian.net/deps/dep14/

## Installation Instructions
```
sudo dpkg -i libminkadaptor0_x_arm64.deb
sudo dpkg -i libminkteec1_x_arm64.deb
sudo dpkg -i minkipc-qteesupplicant_x_arm64.deb
```
## Usage

Build: To build the the package, go to *Actions* tab, select the *Build Debian Package* workflow, then 'Run workflow'

Upstream Version Promotion: ...

The workflows of this repo use the reusable workflows from qcom-build-utils in the background. To understand more how everything
connects together, see https://github.com/qualcomm-linux/qcom-build-utils

## Development

How to develop new features/fixes for the software. Maybe different than "usage". Also provide details on how to contribute via a [CONTRIBUTING.md file](CONTRIBUTING.md).

## Getting in Contact

How to contact maintainers. E.g. GitHub Issues, GitHub Discussions could be indicated for many cases. However a mail list or list of Maintainer e-mails could be shared for other types of discussions. E.g.

* [Report an Issue on GitHub](../../issues)
* [Open a Discussion on GitHub](../../discussions)

## License

pkg-minkipc is licensed under the [BSD-3-clause License](https://spdx.org/licenses/BSD-3-Clause.html). See [LICENSE.txt](LICENSE.txt) for the full license text.
