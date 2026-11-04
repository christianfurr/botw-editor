# Third-party notices

BOTW Editor's original source and artwork are licensed under the [MIT License](LICENSE).
The notices below apply to the identified upstream material.

## MacNativeGames

The bridge integrates with [MacNativeGames](https://github.com/ly05010419/MacNativeGames) at commit `b1a49a2`. Patches may include context from its runtime source.

```text
MIT License

Copyright (c) 2026 MacNativeGames contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

MacNativeGames has additional dependencies and notices in its own repository. The editor release does not redistribute those game-runtime dependencies or a built game app.

## Item catalog

The item actor IDs and English names are derived from Marc Robledo's [BOTW save editor data](https://github.com/marcrobledo/savegame-editors/blob/master/zelda-botw/zelda-botw.data.js), which credits MrCheeze's datamining. No game art from that editor is included.

The upstream catalog notice is preserved below and in the bundled catalog license:

```text
MIT License

Copyright (c) 2017-2023 Marc Robledo

This project incorporates components from Octicons
(https://github.com/primer/octicons/) Copyright (c) 2023 GitHub Inc.,
also released under MIT license.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Runtime research references

Public reverse-engineering references helped identify version-specific function addresses and layouts:

- [Zelda Reverse Engineering Team](https://github.com/zeldaret/botw)
- [Pistonight/botw-save-state](https://github.com/Pistonight/botw-save-state)
- [Pistonight/botw-symbols](https://github.com/Pistonight/botw-symbols)

The bridge implementation is original code. Referenced address/layout facts do not incorporate the GPL implementation from botw-save-state or botw-symbols.

## Artwork and trademarks

The editor frame is original generated artwork created for this project. It is not a game screenshot or extracted Nintendo asset. Interface controls use system-provided macOS components and symbols.

The Legend of Zelda, Breath of the Wild, and Nintendo Switch names identify compatibility. Nintendo does not sponsor or endorse this project.
