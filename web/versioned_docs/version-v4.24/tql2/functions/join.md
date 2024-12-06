# join

Joins a list of strings into a single string using a separator.

```tql
join(xs:list, [separator:string]) -> string
```

## Description

The `join` function concatenates the elements of the input list `xs` into a
single string, separated by the specified `separator`.

### `xs: list`

A list of strings to join.

### `separator: string (optional)`

The string used to separate elements in the result.

Defaults to `""`.

## Examples

### Join a list of strings with a comma

```tql
from {x: join(["a", "b", "c"], "-")}
```

```tql
{x: "a-b-c"}
```

## See Also

[`split`](split.md), [`split_regex`](split_regex.md)