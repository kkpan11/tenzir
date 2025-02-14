---
title: Matching YARA Rules in Byte Pipelines
authors:
   - mavam
date: 2023-11-01
last_updated: 2023-12-12
tags: [yara, operator, dfir, detection engineering]
comments: true
---

The new `yara` operator matches [YARA][yara] rules on bytes, producing a
structured match output to conveniently integrate alerting tools or trigger next
processing steps in your detection workflows.

[yara]: https://virustotal.github.io/yara/

![YARA Operator](yara-operator.excalidraw.svg)

<!--truncate-->

[YARA][yara] rules are a bedrock piece when it comes to writing detections on
binary data. Malware analysts develop them based on sandbox results or threat
reports, incident responders capture the attacker's toolchain on disk images or
in memory, and security engineers share them with their peers.

## Operationalize YARA rules

The most straight-forward way to execute a YARA rule is the official [`yara`
command-line utility](https://yara.readthedocs.io/en/stable/commandline.html).
Consider this rule:

```
rule test {
  meta:
    string = "string meta data"
    integer = 42
    boolean = true

  strings:
    $foo = "foo"
    $bar = "bar"
    $baz = "baz"

  condition:
    ($foo and $bar) or $baz
}
```

Running `yara -g -e -s -L test.yara test.txt` on a file `test.txt` with contents
`foo bar` yields the following output:

```
default:test [] test.txt
0x0:3:$foo: foo
0x4:3:$bar: bar
```

There are other ways to execute YARA rules, e.g.,
[ClamAV](https://www.clamav.net/),
[osquery](https://osquery.readthedocs.io/en/stable/deployment/yara/), or
[Velociraptor](https://docs.velociraptor.app/vql_reference/plugin/yara/)—which
we also [integrated as pipeline
operator](/blog/integrating-velociraptor-into-tenzir-pipelines).

And now there's also Tenzir, with a [`yara`][yara-operator] operator that
accepts bytes as input and produces events as output. Let's take the simple case
of running the above example on string input:

```bash
echo 'foo bar' | tenzir 'load stdin | yara /tmp/test.yara'
```

The operator generates one `yara.match` event per matching rule:

```json
{
  "rule": {
    "identifier": "test",
    "namespace": "default",
    "tags": [],
    "meta": {
      "string": "string meta data",
      "integer": 42,
      "boolean": true
    },
    "strings": {
      "$foo": "foo",
      "$bar": "bar",
      "$baz": "baz"
    }
  },
  "matches": {
    "$foo": [
      {
        "data": "Zm9v",
        "base": 0,
        "offset": 0,
        "match_length": 3
      }
    ],
    "$bar": [
      {
        "data": "YmFy",
        "base": 0,
        "offset": 4,
        "match_length": 3
      }
    ]
  }
}
```

Each match has a `rule` field describing the rule and a `matches` record indexed
by string identifier to report a list of matches per rule string. E.g., there is
one match for `$bar` at byte offset 4 and match length 3. The Base64-encoded
excerpt for the match is `YmFy` (= `"bar"`).[^1]

[^1]: JSON doesn't distinguish binary blobs from strings. However, our type
    system does, so we encode blob values as Base64-encoded strings for formats
    that do not have a native blog representation.

## Building a YARA streaming engine

:::note Implementation Details
You can skip this section if you are not interested in the inner workings, but
it may help understand how YARA works under the hood.
:::

Tenzir byte pipelines consist of a stream of variable-size chunks of memory.
E.g., when loading the raw bytes of file via `load file`, the dataflow may
consist of multiple chunks. YARA scanners can also operate on multiple blocks of
data. It might be tempting to treat these as contiguous, adjacent blocks of
memory (we did this initially) and think that it should be possible to match a
rule across adjacent a blocks, like this:

![YARA scanner blocks](yara-implementation.excalidraw.svg)

[This is not the case](https://github.com/VirusTotal/yara/issues/1994). While it
*may* work, it's possible to write rules where this fails. As a result, simply
keeping the input blocks in memory and feeding them to a scanner *might cause
false negatives* if you have a rule that should match across chunk boundaries.
In other words, it's not possible to build an incremental streaming engine with
the current YARA architecture. Moreover, YARA may perform multiple passes over
the input, so it's neither possible to construct a one-pass streaming engine.

This is the reason why the `yara` operator supports two modes of operation:

1. **Accumulating**: Accumulate all chunks perform a scan at the end. (default)
2. **Blockwise**: scan each block of memory as self-contained unit.
   (`--blockwise`)

Mode (1) copies all chunks in a single buffer. Mode (2) does work in streaming
mode, but it only makes sense if each chunk of memory is a self-contained unit,
e.g., when getting memory chunks from a message broker.

## Mix and match loaders

The `stdin` loader in the above example produces chunks of bytes. But you can
use any connector of your choice that yields bytes. In particular, you can use
the `file` loader:

```bash
tenzir 'load file --mmap /tmp/test.txt | yara /tmp/test.yara'
```

:::note Memory-mapping files
Passing `--mmap` to the `file` loader is purely an optimization that results in
the creation of a single memory block as input to the `yara` operator. This
means the YARA scanner doesn't have to iterate over multiple blocks of memory,
which may be beneficial for intricate rules that require random access into the
file.
:::

If you have a ZeroMQ socket where you publish malware samples to be scanned,
then you only need to change the pipeline source:

```bash
tenzir 'load zmq | yara /tmp/test.yara'
```

This is where the [separation between structured and unstructured
data][separation-of-concerns] in pipelines pays off. You plug in any loader
while leaving the remainder of `yara` pipeline in place.

[separation-of-concerns]: /blog/five-design-principles-for-building-a-data-pipeline-engine#p1-separation-of-concerns

## Post-process matches

Because the matches are structured events, you can use all existing operators to
post-process them. For example, send them to a Slack channel via
`fluent-bit`:

```
load file --mmap /tmp/test.txt
| yara /tmp/test.yara
| fluent-bit slack webhook=URL
```

Or store them with `import` at a Tenzir node to generate match statistics later
on:

```
load file --mmap /tmp/test.txt
| yara /tmp/test.yara
| import
```

## Create a YARA rule matching service

Using just a few pipelines, you can quickly deploy a YARA rule scanning service
that sends the matches to a Slack webhook. Let's that you want to scan malware
sample that you receive over a Kafka topic `malware`. Launch the processing
pipeline as follows:

```
load kafka --topic malware
| yara --blockwise /path/to/rules
| fluent-bit slack webhook=URL
```

This pipeline requires that every Kafka message is a self-contained malware
sample. Because the pipeline runs continuously, we supply the `--blockwise`
option so that the `yara` triggers a scan for every Kafka message, as opposed to
accumulating all messages indefinitely and only initiating a scan when the input
exhausts.

You can now submit a malware sample by sending it to the `malware` Kafka topic:

```
load file --mmap evil.exe | save kafka --topic malware
```

The matches should now arrive as JSON message in the Slack channel associated
with the webhook.

## Summary

We've introduced the `yara` operator as a byte-to-events transformation that
exposes YARA rule matches as structured events, making them easy to post-process
with the existing collection of Tenzir operators. We also explained how you can
create a simple YARA rule scanning service that accepts malware samples via
Kafka and sends the matches to a Slack channel.

Try it yourself. Deploy detection pipelines with the `yara` operator for free
with our Community Edition at [app.tenzir.com](https://app.tenzir.com). Missing
any other operators that operationalize detections? Swing by our [Discord
server](/discord) and let us know!

:::note Acknowledgements
Thanks to [Thomas Patzke](https://github.com/thomaspatzke) for reviewing this
blog post and suggesting to make the default behavior of the operator more safe
to use. 🙏
:::
