param([string]$Root)
$ErrorActionPreference='Stop'
# Every released document, including all Windows 98 windows (Stage 21c): each string a player can read comes
# from the localization catalog through data-l10n.
$files=@(
 'content/rmlui/documents/app_shell.rml','content/rmlui/screens/main_menu.rml','content/rmlui/screens/new_game.rml',
 'content/rmlui/screens/load_game.rml','content/rmlui/screens/loading.rml','content/rmlui/screens/settings.rml',
 'content/rmlui/screens/pause_menu.rml','content/rmlui/modals/confirm_destructive.rml','content/rmlui/screens/game_hud.rml',
 'content/rmlui/screens/orders_tools.rml','content/rmlui/screens/inspector.rml','content/rmlui/windows/workshop_manager.rml',
 'content/rmlui/windows/stockpile_manager.rml','content/rmlui/panels/agriculture_manager.rml','content/rmlui/windows/population_manager.rml',
 'content/rmlui/windows/inventory_browser.rml','content/rmlui/windows/military_manager.rml','content/rmlui/windows/diplomacy_missions.rml')
# These are controls/fixtures whose glyph itself is the contract, not translatable prose.
$allow=[ordered]@{'!'='status icon';'Z-'='level decrement glyph';'Z+'='level increment glyph';'00:00'='dynamic clock fixture';'&#x00D7;'='caption Close glyph'}
$remaining=@()
foreach($relative in $files){
 $text=[IO.File]::ReadAllText((Join-Path $Root $relative),[Text.UTF8Encoding]::new($false,$true))
 foreach($match in [regex]::Matches($text,'<(?<tag>[a-z0-9]+)(?<attrs>(?![^>]*data-l10n=)[^>]*)>(?<value>[^<>]*\S[^<>]*)</\k<tag>>')){
  # The head <title> names the document for tools; it is not shown in the interface.
  if($match.Groups['tag'].Value -eq 'title'){continue}
  $value=$match.Groups['value'].Value
  if(!$allow.Contains($value)){$remaining+="$relative :: $value"}
 }
 foreach($match in [regex]::Matches($text,'<input(?<attrs>(?![^>]*data-l10n-placeholder=)[^>]*)placeholder="(?<value>[^"]+)"[^>]*>')){
  $remaining+="$relative :: placeholder $($match.Groups['value'].Value)"
 }
}
if($remaining.Count){$remaining|ForEach-Object{Write-Host "Unkeyed player-facing literal: $_"};exit 1}
Write-Host "Released literal verifier passed: 0 unkeyed player-facing literals in $($files.Count) documents; $($allow.Count) documented technical fixtures."
