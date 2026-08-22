param([string]$Root)
$ErrorActionPreference='Stop'
$files=@(
 'content/rmlui/documents/app_shell.rml','content/rmlui/screens/main_menu.rml','content/rmlui/screens/loading.rml',
 'content/rmlui/screens/settings.rml','content/rmlui/modals/confirm_destructive.rml','content/rmlui/screens/game_hud.rml',
 'content/rmlui/screens/inspector.rml','content/rmlui/windows/workshop_manager.rml',
 'content/rmlui/windows/stockpile_manager.rml','content/rmlui/panels/agriculture_manager.rml')
# These are controls/fixtures whose glyph itself is the contract, not translatable prose.
$allow=[ordered]@{'!'='status icon';'Z-'='level decrement glyph';'Z+'='level increment glyph';'00:00'='dynamic clock fixture'}
$remaining=@()
foreach($relative in $files){
 $text=[IO.File]::ReadAllText((Join-Path $Root $relative),[Text.UTF8Encoding]::new($false,$true))
 foreach($match in [regex]::Matches($text,'<(?<tag>[a-z0-9]+)(?<attrs>(?![^>]*data-l10n=)[^>]*)>(?<value>[^<>]*\S[^<>]*)</\k<tag>>')){
  $value=$match.Groups['value'].Value
  if(!$allow.Contains($value)){$remaining+="$relative :: $value"}
 }
 foreach($match in [regex]::Matches($text,'<input(?<attrs>(?![^>]*data-l10n-placeholder=)[^>]*)placeholder="(?<value>[^"]+)"[^>]*>')){
  $remaining+="$relative :: placeholder $($match.Groups['value'].Value)"
 }
}
if($remaining.Count){$remaining|ForEach-Object{Write-Error "Unkeyed player-facing literal: $_"};exit 1}
Write-Host "Released literal verifier passed: 0 unkeyed player-facing literals; $($allow.Count) documented technical fixtures."
