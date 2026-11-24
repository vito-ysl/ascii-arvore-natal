# README — Árvore de Natal Animada em ASCII

## 1. Finalidade

Este projeto apresenta, em linguagem C++17, uma composição animada de uma árvore de Natal em caracteres ASCII. A cena inclui noite estrelada, lua crescente, brilho discreto ao fundo, neve em duas camadas, enfeites, guirlandas, presentes e um pequeno boneco de neve.

O programa foi concebido para ocupar integralmente a área útil do terminal em que é executado, manter o desenho centralizado e adaptar-se automaticamente quando a janela é redimensionada. Não há dependências externas: apenas a biblioteca padrão do C++ é utilizada.

O arquivo-fonte `arvore_natal.cpp` é composto exclusivamente por caracteres ASCII, inclusive nos comentários.

---

## 2. Funcionalidades

| Funcionalidade | Descrição |
| --- | --- |
| Árvore em três camadas | Copa construída por interpolação linear da meia-largura em cada linha. |
| Céu estático | Estrelas, lua e brilho de fundo não piscam nem cintilam. |
| Neve suave | Flocos em velocidades distintas, com leve deslocamento lateral e duas profundidades. |
| Brilho de fundo | Halo âmbar discreto atrás da árvore e ao redor da lua. |
| Centralização | O eixo da árvore é sempre a coluna central do terminal. |
| Preenchimento da tela | degradê noturno aplicado em todas as linhas e colunas. |
| Redimensionamento | O tamanho do terminal é consultado a cada quadro; a cena é reconstruída quando necessário. |
| 30 quadros por segundo | Temporização baseada em `std::chrono` com `sleep_until`. |
| Cores ANSI | Cores RGB de 24 bits, com fundo em degradê e primeiro plano por elemento. |
| Modo sem cores | Opção `--plain` para saída textual simples. |
| Modo estático | Opção `--still` para gerar um único quadro. |

---

## 3. Requisitos

- Compilador compatível com C++17, como `g++` 7 ou superior, `clang++` equivalente ou compilador da Microsoft com suporte a C++17.
- Terminal com suporte a sequências ANSI. Terminais modernos em Linux, macOS e Windows atendem a esse requisito.
- Fonte monoespaçada recomendada, para que a largura de um caractere corresponda a uma coluna.

Não são necessários bibliotecas adicionais, arquivos de configuração, conexão de rede ou permissões administrativas.

---

## 4. Compilação e execução

### 4.1 Compilação

```bash
g++ -std=c++17 -O2 -Wall -Wextra arvore_natal.cpp -o arvore_natal
```

### 4.2 Execução

```bash
./arvore_natal
```

A animação permanece em execução até que o usuário pressione `Ctrl+C`. A rotina de interceptação de sinal restaura o cursor, as cores padrão e limpa a tela antes de encerrar.

### 4.3 Opções de linha de comando

| Opção | Efeito |
| --- | --- |
| `--still` | Gera um único quadro e encerra. Não esconde o cursor nem limpa a tela. |
| `--plain` | Desativa sequências de cor ANSI, produzindo texto simples. |
| `--still --plain` | Combina as duas opções; adequado para copiar o desenho para um documento. |

Exemplo:

```bash
./arvore_natal --still --plain > quadro.txt
```

Quando a saída é redirecionada para um arquivo ou pipe, a detecção de tamanho utiliza as variáveis de ambiente `COLUMNS` e `LINES`, quando definidas, e o valor padrão de 100×42 colunas/linhas caso contrário.

---

## 5. Arquitetura do código

### 5.1 Fluxo geral

A cada quadro, o programa executa as seguintes etapas:

1. Consulta o tamanho atual do terminal.
2. Se o tamanho mudou, recalcula o layout e reconstrói estrelas e flocos de neve.
3. Cria uma matriz de células com as dimensões atuais.
4. Desenha, nessa ordem: brilho de fundo, estrelas, neve posterior, árvore e neve anterior.
5. Renderiza a matriz na saída padrão com posicionamento explícito de cada linha.
6. Avança a neve e aguarda até o instante do próximo quadro, mantendo 30 fps.

Essa separação entre cálculo de layout, desenho e renderização evita que a lógica visual dependa da saída e facilita a manutenção.

### 5.2 Estruturas de dados

| Estrutura | Finalidade |
| --- | --- |
| `Size` | Armazena colunas e linhas informadas pelo terminal. |
| `Layout` | Concentra todas as posições derivadas do tamanho: centro, copa, camadas, tronco, guirlandas, presentes e solo. |
| `Cell` | Representa uma posição da tela com seu caractere e cor. |
| `Star` | Posição fixa de uma estrela. |
| `Flake` | Posição, velocidade, fase e caractere de um floco de neve. |
| `Canvas` | `std::vector<Cell>` que representa a tela completa. |

### 5.3 Funções principais

| Função | Responsabilidade |
| --- | --- |
| `terminalSize()` | Determina a resolução do terminal. Usa `TIOCGWINSZ` em sistemas POSIX e `GetConsoleScreenBufferInfo` no Windows. |
| `fallbackSize()` | Fornece o tamanho quando a consulta direta não está disponível. |
| `enableWindowsAnsi()` | Habilita o processamento de sequências ANSI no console moderno do Windows. |
| `updateLayout()` | Recalcula todas as medidas e posições a partir das dimensões recebidas. |
| `treeHalfAt()` | Devolve a meia-largura da árvore em uma linha específica, por interpolação entre as camadas. |
| `buildScene()` | Gera estrelas e flocos em quantidade proporcional à área da tela. |
| `drawGlow()` | Desenha o halo de fundo e a lua crescente. |
| `drawStars()` | Desenha estrelas com brilho fixo e algumas estrelas maiores com raios. |
| `drawGift()` | Desenha um presente com laço e fita vertical. |
| `drawTree()` | Desenha tronco, copa, guirlandas, enfeites, estrela do topo, presentes e solo. |
| `drawSnow()` | Desenha os flocos de uma das duas camadas. |
| `advanceSnow()` | Atualiza a posição dos flocos e os faz reaparecer no topo ao sair da tela. |
| `ansi()` | Traduz a enumeração de cores em sequências ANSI RGB. |
| `background()` | Gera a cor de fundo do degradê para uma determinada linha. |
| `put()` | Escreve com segurança uma célula, descartando coordenadas fora da tela. |
| `render()` | Converte a matriz em texto, posicionando cada linha e alternando cores apenas quando necessário. |
| `cleanup()` | Restaura o estado do terminal ao receber `SIGINT` ou `SIGTERM`. |

---

## 6. Detalhes de implementação

### 6.1 Detecção de tamanho e múltiplos monitores

O programa não enumera monitores. Ele consulta o terminal no qual foi iniciado, por meio da saída padrão:

- Em Linux e macOS, `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)` fornece colunas e linhas.
- No Windows, `GetConsoleScreenBufferInfo` fornece as dimensões da janela do console.

Assim, o desenho acompanha corretamente o terminal em qualquer monitor, inclusive em configurações com múltiplas telas de resoluções diferentes. Se a janela for redimensionada, a consulta seguinte já detecta a mudança e o layout é recalculado antes do próximo quadro.

### 6.2 Layout responsivo e centralização

Todas as medidas são derivadas de `L.w` e `L.h`. O centro horizontal é `L.cx = L.w / 2`. A altura da copa, o início de cada camada e a meia-largura máxima são proporcionais à altura e à largura disponíveis.

A função `treeHalfAt()` interpola a largura linearmente dentro de cada camada, produzindo o formato triangular. Para telas muito pequenas, há limites mínimos de 20 colunas e 10 linhas, a fim de evitar valores inválidos. Nessas situações, a árvore reduz-se proporcionalmente.

A quantidade de estrelas e flocos também é proporcional à área e à largura, respectivamente, com limites superiores para preservar o desempenho.

### 6.3 Renderização sem flicker

A tela é limpa apenas uma vez no início e, excepcionalmente, quando há redimensionamento. Em cada quadro, o cursor volta à origem e cada linha é posicionada explicitamente com uma sequência do tipo `\033[linha;1H`.

Essa abordagem apresenta duas vantagens:

- evita o `clear` completo a cada quadro, principal causa de cintilação visual;
- impede que a escrita na última coluna provoque quebra de linha e rolagem indesejada.

A cor de primeiro plano só é reemitida quando muda de uma célula para outra, reduzindo o volume de sequências de controle.

### 6.4 Animação

O intervalo de quadro é de 33.333 microssegundos, correspondente a 30 fps. A temporização usa `std::chrono::steady_clock` e `sleep_until`, o que evita acúmulo de atrasos. Se o processamento exceder o intervalo, o relógio é realinhado para impedir uma sequência de quadros apressados.

As estrelas são fixas. A única animação é a queda da neve, com velocidade escalada conforme a altura do terminal e leve oscilação horizontal determinada por uma função senoidal.

### 6.5 Cores e caracteres

As cores são definidas em RGB de 24 bits por meio de sequências ANSI `38;2;R;G;B` para primeiro plano e `48;2;R;G;B` para fundo. O fundo forma um degradê azul, mais escuro no topo e mais claro junto ao solo.

O desenho emprega somente caracteres ASCII imprimíveis, como `*`, `+`, `.`, `'`, `,`, `:`, `-`, `|`, `/`, `\`, `#`, `%`, `@`, `&`, `^`, `~`, `o`, `O`, `0`, `8`, `$`, `=` e `(`.

---

## 7. Processo de escrita

O código foi desenvolvido de forma incremental, em quatro etapas:

1. **Composição inicial em UTF-8.** A primeira versão usava glifos decorativos, como flocos de neve e símbolos de enfeites. O objetivo era validar a geometria da árvore, a disposição do céu e a sobreposição da neve.
2. **Conversão para ASCII.** A pedido, todos os glifos foram substituídos por caracteres ASCII comuns. Os comentários também foram convertidos, de modo que o arquivo-fonte inteiro passou a ser ASCII puro.
3. **Estabilização visual.** O cintilar das estrelas e a pulsação da estrela do topo foram removidos. A limpeza integral de tela foi substituída por reposicionamento de cursor e o intervalo de quadro foi fixado em 30 fps. Nessa etapa, acrescentou-se o degradê noturno.
4. **Adaptação ao terminal.** As constantes de largura e altura foram substituídas pelo par de funções `terminalSize()` e `updateLayout()`. A cena passou a ser reconstruída quando o tamanho muda, e a árvore passou a ser dimensionada proporcionalmente.

As decisões de projeto priorizaram clareza, determinismo e portabilidade. A geração aleatória utiliza uma semente fixa, de modo que uma mesma resolução produz sempre a mesma disposição de estrelas e flocos. As funções foram mantidas pequenas e com responsabilidades distintas, e o estado global foi limitado ao layout e à preferência de cores.

---

## 8. Considerações éticas

- **Privacidade:** o programa não coleta, armazena, transmite ou processa dados pessoais. Não abre conexões de rede nem acessa arquivos além da leitura das variáveis de ambiente `COLUMNS` e `LINES`.
- **Integridade do sistema:** a aplicação apenas escreve na saída padrão. Não altera arquivos, configurações nem o estado do sistema. O cursor e as cores do terminal são restaurados ao final.
- **Acessibilidade:** o céu permanece estático para reduzir estímulos luminosos. As opções `--still` e `--plain` permitem dispensar movimento e cores, respectivamente, conforme a preferência ou necessidade do usuário.
- **Respeito ao ambiente:** o programa não presume características do monitor nem do usuário. Adapta-se ao terminal efetivamente em uso e mantém limites de consumo de processamento.
- **Conteúdo:** a composição é decorativa e não ofensiva. Não incorpora material de terceiros nem dependências externas.
- **Responsabilidade de uso:** o código é fornecido no contexto deste projeto. Redistribuição ou incorporação em outros trabalhos deve observar as condições definidas pelo responsável pela entrega.

---

## 9. Limitações conhecidas

- Terminais que não interpretam sequências ANSI exibem o desenho com os caracteres de controle visíveis. Nesses casos, recomenda-se `--plain`.
- Terminais com menos de 20 colunas ou 10 linhas são tratados com esses valores mínimos; linhas adicionais podem rolar.
- A largura de um caractere é considerada igual a uma coluna. Fontes não monoespaçadas podem comprometer o alinhamento.
- A detecção considera o terminal ativo, não a resolução física do monitor. Essa distinção é intencional: o desenho deve ocupar a área de texto disponível, e não a tela inteira do display.

---

## 10. Verificação realizada

- Compilação com `-std=c++17 -O2 -Wall -Wextra`, sem avisos.
- Verificação do arquivo-fonte quanto à presença exclusiva de caracteres ASCII.
- Testes de saída em 25×10, 60×20, 80×24 e 140×45 colunas/linhas, com todas as linhas preenchidas até a última coluna.
- Medição de aproximadamente 30 quadros por segundo em execução contínua.
- Conferência dos modos `--still` e `--plain`.

---

## 11. Estrutura de arquivos

```text
.
├── arvore_natal.cpp   Código-fonte completo, em C++17 e ASCII puro.
└── README.md          Este documento.
```

O executável gerado pela compilação não é versionado e deve ser produzido localmente conforme a seção 4.
