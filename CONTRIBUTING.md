<!---
Copyright © 2025 Arm Limited.
SPDX-License-Identifier: MIT
--->

# Contribution Guidelines

The Arm Accuracy Super Resolution (Arm ASR) project is open for external contributors and welcomes contributions.
Arm ASR is licensed under the [MIT](https://spdx.org/licenses/MIT.html) license and all accepted contributions must have the same license.

Below is an overview on contributing code to Arm ASR.

## Contributing code to Arm ASR

- Before the Arm ASR project accepts your contribution, you need to certify its origin and give us your permission. To manage this process we use
  [Developer Certificate of Origin (DCO) V1.1](https://developercertificate.org/).
  To indicate that contributors agree to the terms of the DCO, it's necessary to "sign off" the
  contribution by adding a line with your name and e-mail address to every git commit message:

  ```log
  Signed-off-by: FIRST_NAME SECOND_NAME <your@email.address>
  ```

  This can be done automatically by adding the `-s` option to your `git commit` command.
  You must use your real name, no pseudonyms or anonymous contributions are accepted

- In each source file, include the following copyright notice:

  ```bash
  # Copyright © <years changes were made> <copyright holder>.
  # SPDX-License-Identifier: MIT
  ```
  Note: if an existing file does not conform, please update the license header as part of your contribution.

## Code Reviews

Contributions must go through code review on GitHub.

See [here](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request-from-a-fork)
on details of how to create a pull request from your working fork.


Only reviewed contributions can go to the main branch of Arm ASR.

## Continuous integration

Contributions to Arm ASR go through testing at the Arm CI system. All unit, integration and regression tests must pass before a contribution gets merged to the Arm ASR main branch.