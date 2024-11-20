---
sidebar_custom_props:
  operator:
    transformation: true
---

# decapsulate

Decapsulates packet data at link, network, and transport layer.

:::warning Deprecated
This operator will soon be removed in favor of first-class support for functions
that can be used in a variety of different operators and contexts.
:::

## Synopsis

```
decapsulate
```

## Description

The `decapsulate` operator proceses events of type `pcap.packet` and
decapsulates the packet payload by extracting fields at the link, network, and
transport layer. The aim is not completeness, but rather exposing commonly used
field for analytics.

The operator only processes events of type `pcap.packet` and emits events of
type `tenzir.packet`.

### VLAN Tags

While decapsulating packets, `decapsulate` extracts
[802.1Q](https://en.wikipedia.org/wiki/IEEE_802.1Q) VLAN tags into the nested
`vlan` record, consisting of an `outer` and `inner` field for the respective
tags. The value of the VLAN tag corresponds to the 12-bit VLAN identifier (VID).
Special values include `0` (frame does not carry a VLAN ID) and `0xFFF`
(reserved value; sometimes wildcard match).

## Examples

Decapsulate packets from a PCAP file:

```
from file /tmp/trace.pcap read pcap
| decapsulate
```

Extract packets as JSON that have the address 6.6.6.6 as source or destination,
and destination port 5158:

```
read pcap
| decapsulate
| where 6.6.6.6 && dport == 5158
| write json
```

Query VLAN IDs using `vlan.outer` and `vlan.inner`:

```
read pcap
| decapsulate
| where vlan.outer > 0 || vlan.inner in [1, 2, 3]
```

Filter packets by [Community
ID](https://github.com/corelight/community-id-spec):

```
read pcap
| decapsulate
| where community_id == "1:wCb3OG7yAFWelaUydu0D+125CLM="
```
