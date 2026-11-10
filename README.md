---
description: "Develop SDGOODS round-badge firmware with standalone hardware records, source templates, UI themes and an Agent skill."
---

# Sugarat SDGOODS development kit

English | [中文](README.zh.md)

## Summary

Give your preferred Agent this kit's GitHub entry link to develop firmware for the SDGOODS ESP32-S3 360×360 round badge. The kit includes a source foundation, Watch/Game themes, 16 component bundles and hardware records. It works without Sugarat or this parent repository. ESP-IDF and its toolchain are installed separately.

## Contents

- [Give this kit to an Agent](#give-this-kit-to-an-agent)
- [Use the sources locally](#use-the-sources-locally)
- [Resources and verification](#resources-and-verification)
- [Publishing](#publishing)

<a id="give-this-kit-to-an-agent"></a>
## Give this kit to an Agent

Copy [COPY_TO_AGENT.txt](COPY_TO_AGENT.txt) into your preferred Agent, then describe the application you want. The sentence points to the fixed [v0.1.0 Agent entry](https://github.com/suoyousheng-bot/sugarat-sdgoods/blob/v0.1.0/AGENTS.md); the complete [v0.1.0 source ZIP](https://github.com/suoyousheng-bot/sugarat-sdgoods/archive/refs/tags/v0.1.0.zip) contains the matching kit. The receiving Agent reads [AGENTS.md](AGENTS.md) and [SKILL.md](SKILL.md), then downloads that version. A supported Skill installer must retain the whole directory, including assets and scripts.

Generate the sentence for an actual publication destination with `python3 scripts/kit.py prompt --repo <repository-url> --ref <release-tag-or-commit> --task <application-request>`. This command formats text; it does not publish or check repository access. The output can be copied by a product clipboard action.

<a id="use-the-sources-locally"></a>
## Use the sources locally

Use Python 3.9 or newer for the kit helper. Run from the kit root:

```sh
python3 scripts/kit.py check
python3 scripts/kit.py components --id sdgoods-ui-watch
python3 scripts/kit.py init --directory ../my-badge --name my_badge --components sdgoods-ui-watch
```

Initialization creates a new firmware project and installs the theme with its Pixel dependency. It leaves application wiring to the Agent and refuses an existing destination. For an existing compatible project, use `python3 scripts/kit.py add --directory ../my-badge --components sdgoods-case-time`; differing files are preserved and reported before component writes begin.

Read [development](references/development.md) and [environment](references/environment.md) before compiling. The helper never installs SDKs, opens a port, flashes, or creates a private backup. Device work follows [deployment](references/deployment.md) and the user's authorization.

<a id="resources-and-verification"></a>
## Resources and verification

[resources.lock.json](resources.lock.json) records versions, dependencies, integration notes and upstream verification limits. [checksums.sha256](checksums.sha256) covers distributed files. The original template archive and its per-source manifest remain intact. [Hardware records](references/hardware.md) describe documented specifications; [components](references/components.md) routes to included sources and examples. [NOTICE.md](NOTICE.md) identifies license owners.

Compilation, written-byte verification, startup and physical acceptance are separate results. Historical acceptance belongs to the recorded application and device; every new integration needs its own checks. This kit includes no prebuilt firmware, device backups, credentials, full SDK, or Sugarat native-preview runtime. Local validation results and remaining platform checks are recorded in [VALIDATION.md](VALIDATION.md).

<a id="publishing"></a>
## Publishing

The public repository is [suoyousheng-bot/sugarat-sdgoods](https://github.com/suoyousheng-bot/sugarat-sdgoods), with the first resource snapshot tagged `v0.1.0`. To publish a fork, copy this entire directory as its repository root and follow [publishing](references/publishing.md) to verify files and regenerate the handoff sentence for your destination.

## Dev Note

None.
