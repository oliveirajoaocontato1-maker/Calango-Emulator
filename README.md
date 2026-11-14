<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->

<p align="center">
  <img src="dist/branding/calango_logo.png" alt="Calango Emulator" width="600">
</p>

# Calango Emulator

Emulador de Nintendo Switch 2 e Switch de código aberto, escrito em C++, ajustado para placas
**AMD Radeon** e feito sob medida para **The Legend of Zelda: Ocarina of Time**. Também funciona em
placas NVIDIA e Intel.

## Download

Baixe a versão mais recente na página de
[**Releases**](https://github.com/oliveirajoaocontato1-maker/Calango-Emulator/releases).

1. Extraia o `.zip` inteiro em uma pasta (não rode de dentro do `.zip`).
2. Abra `calango.exe`.
3. Clique em **+ Adicionar jogos**, escolha a pasta do seu jogo e dê play.

Tudo já vem configurado para o melhor desempenho: FSR 3.1 com Native AA, geração de quadros
embutida no emulador, apresentação assíncrona, filtro anisotrópico 16x e FPS sem trava.
O arquivo `LEIA-ME.txt` dentro do pacote tem o passo a passo e os requisitos.

Use apenas jogos que você obteve legalmente. Jogos, chaves e firmware não estão incluídos.

## O que tem de diferente

- Visual próprio: jade sobre basalto, fonte Inter, emblema de calango e tela inicial em português.
- Geração de quadros embutida (FidelityFX), sem depender do runtime da AMD, que trava o driver de
  algumas RX 6000.
- Apresentação assíncrona por padrão: o vsync não segura a thread de vídeo (cerca de 18% mais FPS
  andando em Ocarina of Time e metade das travadas).
- Sem serviços online: sem verificação de atualizações e sem Discord.
- Compilado com clang-cl, ThinLTO e PGO.

Detalhes técnicos (em inglês): [README.txt](README.txt) e [CALANGO.md](CALANGO.md).

## Compilar

Veja o [guia de compilação](docs/Build.md). O alvo da interface continua se chamando `yuzu`; o
executável gerado é `calango.exe` no Windows.

## Créditos e licença

Calango Emulator é software livre sob a GNU General Public License v3 ou posterior
([LICENSE.txt](LICENSE.txt), [NOTICE.txt](NOTICE.txt)). Ele se baseia no trabalho destes projetos e
mantém seus avisos de copyright e licenças:

- [Lichii](https://github.com/zortos293/Lichii), de zortos293 e colaboradores;
- Eglantine (desenvolvedora original Camille LaVey), Eden, yuzu e Sudachi;
- o trabalho de desempenho para AMD de [conongnang](https://github.com/conongnang/Lichii),
  [Fritchii](https://github.com/Frizti/Fritchii) e [Nyamu](https://github.com/Spikol02/Nyamu);
- o [FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) da AMD, para FSR e
  geração de quadros;
- a fonte [Inter](https://rsms.me/inter/) (SIL Open Font License 1.1).

Calango Emulator não tem ligação com a Nintendo.
