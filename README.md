# tile-smush

This is a hacked up fork of [tilemaker](https://github.com/systemed/tilemaker).

It lets you do:

```bash
tile-smush foo.mbtiles bar.mbtiles
```

and get a `merged.mbtiles` that is the concatenation of the layers in the input files.

It's meant to work on mbtiles produced by [mapt](https://github.com/cldellow/mapt/). These mbtiles may have overlapping tiles, but the tiles will not have overlapping layers.

This means they can be merged by just concatenating the protobufs, which in theory is a mechanical transformation that should be able to be done very quickly.

## Caveats

This was very much built just to enable my personal use case: using [mapt](https://github.com/cldellow/mapt/) to generate thematic layers, then merging them into a single file.

As such, it prioritizes speed and "just enough to work".

- If two or more input mbtiles files contain the same layers, the result
  is undefined.
- The `json` metadata value will get its `vector_tiles` entries merged,
  but any other entries (such as `tilestats`) are just dropped on the floor.
  - This JSON munging is done via string manipulation. It's a total hack,
    but it works. If someone wants to send a PR using rapidjson, it'd be welcome.

## Alternatives

You can also use [tile-join](https://github.com/mapbox/tippecanoe) from Mapbox's tippecanoe. It has a much broader scope, which comes at the cost of being 10-50x slower.

## Installing

See tilemaker's instructions.

## Copyright

Large chunks of tile-smush come from tilemaker. See its README.md for additional copyright and licensing information.

tile-smush is licensed as FTWPL; you may do anything you like with this code and there is no warranty.
