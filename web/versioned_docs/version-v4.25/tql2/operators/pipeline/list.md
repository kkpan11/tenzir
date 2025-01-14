# list

Shows managed pipelines.

```tql
package::list
```

## Description

The `pipeline::list` operator returns the list of all managed pipelines. Managed
pipelines are pipelines created through the [`/pipeline` API](/api), which
includes all pipelines run through the Tenzir Platform.

## Examples

### Count pipelines per state

```tql
package::list
top state
```

```tql
{
  "state": "running",
  "count": 31
}
{
  "state": "failed",
  "count": 4
}
{
  "state": "stopped",
  "count": 2
}
```

### Show pipelines per package

```tql
package::list
summarize package, names=collect(name)
```

```tql
{
  "package": "suricata-ocsf",
  "names": [
    "Suricata Flow to OCSF Network Activity",
    "Suricata DNS to OCSF DNS Activity",
    "Suricata SMB to OCSF SMB Activity",
    // …
  ]
}
```

## See Also

[`package::list`](../package/list.md)