# Refatoracao das paginas web (hardcoded)

## Objetivo
- Melhorar a organizacao do HTML, CSS e JS dentro do firmware
- Reutilizar estilos e utilitarios comuns
- Padronizar elementos entre paginas
- Melhorar responsividade em portrait e landscape
- Manter o fluxo atual de update de firmware (sem LittleFS)

## Escopo
- Extrair paginas para arquivos dedicados em `src/WebServer/Pages/`
- Criar layout, estilos e scripts comuns
- Ajustar responsividade e padronizacao visual
- Manter rotas HTTP existentes

## Estrutura proposta
- `CommonLayout.h`
- `CommonStyles.h`
- `CommonScripts.h`
- `DashboardPage.h`
- `ConfigPage.h`
- `TelemetryPage.h`
- `FirmwarePage.h`
- `LogsPage.h`
- `LegacyIndexPage.h` (nao utilizado por rotas, apenas referencia)

## Padrao visual
- Usar o Dashboard como baseline
- Unificar cores, fontes, espacamentos, bordas e sombras
- Unificar topbar e estados de navegacao
- Unificar tabelas, cards e botoes

## Responsividade
- Mobile portrait-first
- Grid com `auto-fit` e `minmax`
- Topbar com quebra de linha
- Tabelas com `overflow-x` em telas menores
- Inputs e botoes full width

## Tokens dinamicos
- `APP_VERSION`, `BUILD_DATE`, `BUILD_TIME`, `CONTROLLER`
- Centralizados em helper `applyCommonTokens`

## Riscos e cuidados
- Garantir que nenhum endpoint HTTP seja alterado
- Manter chamadas de API existentes
- Verificar se o JS continua apontando para os mesmos elementos

## Testes manuais
- Acessar `/`, `/config`, `/telemetry`, `/firmware`, `/logs-page`
- Validar update de firmware, salvar configuracao, e logs
- Verificar layout em 360x640, 640x360 e 1024x768
