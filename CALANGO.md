# Calango Emulator

Emulador de Nintendo Switch 2 e Switch, focado em placas AMD Radeon e feito sob medida para
The Legend of Zelda: Ocarina of Time. Compilado com clang-cl, ThinLTO e otimização guiada por
perfil (PGO).

## New in build 1

- **Calango Emulator v1.0**: nome, emblema e visual novos (jade sobre basalto, fonte Inter,
  cantos suaves, barra de carregamento em escamas) e tela inicial em português.
- **Já vem configurado para o melhor desempenho**: é só adicionar a pasta dos jogos e dar play.
  - AMD FSR 3.1 com Native AA: imagem nítida em 1080p.
  - Geração de quadros embutida no emulador (EDEN_NX2_FFX_FG=1 em nx2_switches.txt), sem
    depender do runtime da AMD (que trava o driver de algumas RX 6000).
  - Apresentação assíncrona: o vsync não segura mais a thread de vídeo (cerca de 18% mais FPS
    andando e metade das travadas no teste de Ocarina of Time).
  - Filtro anisotrópico 16x e nitidez do FSR ajustada; FPS sem trava.
- **Sem serviços online**: sem verificação de atualizações e sem Discord; a lista de capas dos
  jogos vem junto (game_pictures.json).

Calango Emulator é software livre sob a GNU GPL v3 (LICENSE.txt). Os projetos em que ele se
baseia e seus autores estão em NOTICE.txt.
