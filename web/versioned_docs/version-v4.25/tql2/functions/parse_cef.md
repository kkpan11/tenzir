# parse_cef

Parses a string as a CEF message

```tql
parse_cef(input:string) -> record
```

## Description

The `parse_cef` function parses a string as a CEF message

### `input: string`

The string to parse.

## Examples

```tql
from { x = "CEF:0|Cynet|Cynet 360|4.5.4.22139|0|Memory Pattern - Cobalt Strike Beacon ReflectiveLoader|8|key=value" }
y = x.cef()
```
```tql
{
  cef_version: 0,
  device_vendor: "Cynet",
  device_product: "Cynet 360",
  device_version: "4.5.4.22139",
  signature_id: "0",
  name: "Memory Pattern - Cobalt Strike Beacon ReflectiveLoader",
  severity: "8",
  extension: {
    key: "value"
  }
}
```