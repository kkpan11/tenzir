# Setup VAST

This section covers the different stages of the setup process that ultimately
yield a running VAST instance. You have several options to enter the setup
pipeline, based on what intermediate artifact you would like to begin with.

```mermaid
flowchart LR
  classDef action fill:#00a4f1,stroke:none,color:#eee
  classDef artifact fill:#bdcfdb,stroke:none,color:#222
  %% Actions
  download(Download):::action
  build(Build):::action
  install(Install):::action
  deploy(Deploy):::action
  configure(Configure):::action
  tune(Tune):::action
  %% Artifacts
  source([Source Code]):::artifact
  binary([Binary]):::artifact
  deployable([Package/Image]):::artifact
  instance([Instance]):::artifact
  %% Edges
  download --> source
  download --> binary
  download --> deployable
  source --> build
  build --> binary
  binary --> install
  install --> deployable
  deployable --> deploy
  deploy --> instance
  instance <--> configure
  instance <--> tune
  %% Links
  click download "/docs/setup-vast/download/" "Download VAST"
  click build "/docs/setup-vast/build/" "Build VAST"
  click install "/docs/setup-vast/install/" "Install VAST"
  click deploy "/docs/setup-vast/deploy/" "Deploy VAST"
  click configure "/docs/setup-vast/configure/" "Configure VAST"
  click tune "/docs/setup-vast/tune/" "Tune VAST"
```

:::tip Quick Start
Want hands-on experience with VAST? Then continue with a quick tour below. 👇
:::

## Run VAST

To get up and running quickly, we recommend using the static binary or Docker
image:

import Tabs from '@theme/Tabs';
import TabItem from '@theme/TabItem';

<Tabs>
<TabItem value="static" label="Static Binary" default>
If you have a Linux at your fingertips, just download and extract our package
with a static binary:

```bash
mkdir /opt/vast
cd /opt/vast
wget https://github.com/tenzir/vast/releases/latest/download/vast-linux-static.tar.gz
mkdir -p /opt/vast
tar xzf vast-linux-static.tar.gz -C /opt/vast
export PATH="/opt/bin/vast:$PATH" # based on your shell, e.g., fish_add_path /opt/bin/vast
vast start
```
</TabItem>
<TabItem value="docker" label="Docker">
For a container deployment, use our official Docker image:

```bash
docker pull tenzir/vast
mkdir -p /tmp/db # persistent state
docker run -dt --name=vast --rm -p 42000:42000 -v /tmp/db:/var/lib/vast \
  tenzir/vast start
```
</TabItem>
</Tabs>

Now that you have running VAST node, you can start ingesting data.

## Ingest Data

import MissingDocumentation from '@site/presets/MissingDocumentation.md';

<MissingDocumentation/>

## Query Data

<MissingDocumentation/>

## Transform Data

<MissingDocumentation/>

## Match Data

<MissingDocumentation/>