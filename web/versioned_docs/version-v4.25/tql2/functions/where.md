# where

Filters list elements based on a predicate.

```tql
where(xs:list, capture:field, predicate:bool) -> list
```

## Description

The `where` function keeps only elements of a list for which a predicate
evaluates to `true`.

### `xs: list`

A list of values.

### `capture: field`

The name of each list element in each predicate.

### `predicate: bool`

The predicate evaluated for each list element.

## Examples

### Keep only elements greater than 3

```tql
from {
  xs: [1, 2, 3, 4, 5]
}
xs = xs.where(x, x > 3)
```

```tql
{xs: [4, 5]}
```

## See Also

[`map`](map.md)