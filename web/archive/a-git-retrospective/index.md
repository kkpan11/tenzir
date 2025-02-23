---
title: A Git Retrospective
authors: mavam
date: 2022-09-15
tags: [git, r, quarto, notebooks, engineering, open-source]
---

The VAST project is roughly a decade old. But what happened over the last 10
years? This blog post looks back over time through the lens of the git *merge*
commits.

Why merge commits? Because they represent a unit of completed contribution.
Feature work takes place in dedicated branches, with the merge to the main
branch sealing the deal. Some feature branches have just one commit, whereas
others dozens. The distribution is not uniform. As of `6f9c84198` on Sep 2,
2022, there are a total of 13,066 commits, with 2,334 being merges (17.9%).
We’ll take a deeper look at the merge commits.

<!--truncate-->

``` bash
cd /tmp
git clone https://github.com/tenzir/vast.git
cd vast
git checkout 6f9c841980b2333028b1ac19e2a21e99d96cbd36
git log --merges --pretty=format:"%ad|%d" --date=iso-strict |
  sed -E 's/(.+)\|.*tag: ([^,)]+).*/\1 \2/' |
  sed -E 's/(.*)\|.*/\1 NA/' \
  > /tmp/vast-merge-commits.txt
```

For the statistics, we’ll switch to R. In all subsequent figures, a single point
corresponds to a merge commit. The reduced opacity alleviates the effects of
overplotting.

<details><summary>Code</summary>

``` r
library(dplyr)
library(ggplot2)
library(lubridate)
library(readr)

theme_set(theme_minimal())

data <- read_table("/tmp/vast-merge-commits.txt",
  col_names = c("time", "tag"),
  col_types = "Tc"
) |>
  arrange(time) |>
  mutate(count = row_number())

first_contribution <- \(x) data |>
  filter(time >= x) |>
  pull(count) |>
  first()

events <- tribble(
  ~time, ~event,
  ymd("2016-03-17"), "NSDI '16\npublication",
  ymd("2017-08-31"), "Tenzir\nincorporated",
  ymd("2018-07-01"), "Tobias",
  ymd("2019-09-15"), "Dominik",
  ymd("2020-01-01"), "Benno",
  ymd("2021-12-01"), "Thomas",
  ymd("2022-07-01"), "Patryk",
) |>
  mutate(time = as.POSIXct(time), count = Vectorize(first_contribution)(time))

data |>
  ggplot(aes(x = time, y = count)) +
  geom_point(size = 1, alpha = 0.2) +
  geom_segment(
    data = events,
    aes(xend = time, yend = count + 200),
    color = "red"
  ) +
  geom_label(
    data = events,
    aes(y = count + 200, label = event),
    color = "red",
    size = 2
  ) +
  scale_x_datetime(date_breaks = "1 year", date_labels = "%Y") +
  labs(x = "Time", y = "Merge Commits")
```

</details>

import Svg1 from './index_files/figure-gfm/full-time-spectrum-1.svg';

<Svg1 />

Prior to Tenzir taking ownership of the project and developing VAST, it was a
dissertation project evolving along during PhD work at the University of
California, Berkeley. We can see that the first pre-submission crunch started a
few months before the [NSDI ’16
paper](https://matthias.vallentin.net/papers/nsdi16.pdf).

Tenzir was born in fall 2017. Real-world contributions arrived as of 2018 when
the small team set sails. Throughput increased as core contributors joined the
team. Fast-forward to 2020 when we started doing public releases. The figure
below shows how this process matured.

<details><summary>Code</summary>

``` r
library(ggrepel)

data |>
  ggplot(aes(x = time, y = count, label = tag)) +
  geom_point(size = 1, alpha = 0.1) +
  geom_text_repel(
    size = 2,
    min.segment.length = 0,
    max.overlaps = Inf,
    segment.color = "red",
    segment.alpha = 0.2,
    box.padding = 0.2
  ) +
  scale_x_datetime(
    date_breaks = "1 year",
    limits = c(as.POSIXct(ymd("2020-01-01")), max(data$time)),
    date_labels = "%Y"
  ) +
  labs(x = "Time", y = "Merge Commits")
```

</details>

import Svg2 from './index_files/figure-gfm/since-2020-1.svg';

<Svg2 />

As visible from the tag labels, we were at [CalVer](https://calver.org) for a
while, but ultimately switched to [SemVer](https://semver.org). Because we had
already commercial users at the time, this helped us better communicate breaking
vs. non-breaking changes.

Let’s zoom in on all releases since v1.0. At this time, we had a solid
engineering and release process in place.

<details><summary>Code</summary>

``` r
library(tidyr)
v1_0_0_rc1_time <- data |>
  filter(tag == "v1.0.0-rc1") |>
  pull(time)

since_v1_0_0_rc1 <- data |> filter(time >= v1_0_0_rc1_time)

rc <- since_v1_0_0_rc1 |>
  drop_na() |>
  filter(grepl("rc", tag))

non_rc <- since_v1_0_0_rc1 |>
  drop_na() |>
  filter(!grepl("rc", tag))

since_v1_0_0_rc1 |>
  ggplot(aes(x = time, y = count, label = tag)) +
  geom_point(size = 1, alpha = 0.2) +
  geom_segment(
    data = non_rc,
    aes(xend = time, yend = min(count)), color = "red"
  ) +
  geom_text_repel(
    size = 2,
    min.segment.length = 0,
    max.overlaps = Inf,
    segment.color = "grey",
    box.padding = 0.7
  ) +
  geom_point(
    data = rc, aes(x = time, y = count),
    color = "blue",
    size = 2
  ) +
  geom_point(
    data = non_rc, aes(x = time, y = count),
    color = "red",
    size = 2
  ) +
  geom_label(data = non_rc, aes(y = min(count)), size = 2, color = "red") +
  scale_x_datetime(date_breaks = "1 month", date_labels = "%b %y") +
  labs(x = "Time", y = "Merge Commits")
```

</details>

import Svg3 from './index_files/figure-gfm/since-v1.0-1.svg';

<Svg3 />

The v2.0 release was a hard one for us, given the long distance to v1.1. We
merged too much and testing took forever. Burnt by the time sunk in testing and
fixups, we decided to switch to an LPU model (“least publishable unit”) to
reduce release cadence. We didn’t manage to implement this model until after
v2.1 though, where the release cadence finally gets smaller. A monthly release
feels about the right for our team size.

The key challenge is minimizing the feature freeze phase. The first release
candidate (RC) kicks this phase off, and the final release lifts the
restriction. In this period, features are not allowed to be merged.[^1] This is
a delicate time window: too long and the fixups in the RC phase cause the
postponed pull requests to diverge, too short and we compromise on testing
rigor, causing a release that doesn’t meet our Q&A requirements.

This is where we stand as of today. We’re happy how far along we came, but
many challenges still lay ahead of us. Increased automation and deeper testing
is the overarching theme, e.g., code coverage, fuzzing, GitOps. We’re constantly
striving to improve or processes. With a small team of passionate, senior
engineers, this is a lot of fun!

[^1]: We enforced this with a `blocked` label. CI [doesn’t allow
    merging](https://github.com/tenzir/vast/blob/6f9c841980b2333028b1ac19e2a21e99d96cbd36/.github/workflows/blocked.yaml) when this label is on a pull request.
