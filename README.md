# tile-smush

This is a hacked up fork of [tilemaker](https://github.com/systemed/tilemaker).

It's eventual goal is to let you do:

```bash
tile-smush foo.mbtiles bar.mbtiles
```

and get a `smushed.mbtiles` that is the concatenation of the layers in the input files.

It's meant to work on mbtiles produced by [mapt](https://github.com/cldellow/mapt/). These mbtiles may have overlapping tiles, but the tiles will not have overlapping layers.

This means they can be merged by just concatenating the protobufs, which in theory is a mechanical transformation that should be able to be done very quickly.

## Alternatives

You can also use [tile-join](https://github.com/mapbox/tippecanoe) from Mapbox's tippecanoe. It seems to do more stuff, and be slower than it could be for this use case.

## Installing

See tilemaker's instructions.

## Copyright

Large chunks of tile-smush come from tilemaker. See its README.md for additional copyright and licensing information.

tile-smush is licensed as FTWPL; you may do anything you like with this code and there is no warranty.
