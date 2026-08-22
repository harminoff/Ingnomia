param([string]$Root = (Resolve-Path "$PSScriptRoot/../.."))
$ErrorActionPreference = 'Stop'
$files = [ordered]@{
 'content/rmlui/documents/app_shell.rml'='shell.app'
 'content/rmlui/screens/main_menu.rml'='shell.main_menu'
 'content/rmlui/screens/loading.rml'='shell.loading'
 'content/rmlui/screens/settings.rml'='shell.settings'
 'content/rmlui/modals/confirm_destructive.rml'='shell.confirm'
 'content/rmlui/screens/game_hud.rml'='hud.static'
 'content/rmlui/screens/inspector.rml'='inspector.static'
 'content/rmlui/windows/workshop_manager.rml'='management.workshop'
 'content/rmlui/windows/stockpile_manager.rml'='management.stockpile'
 'content/rmlui/panels/agriculture_manager.rml'='management.agriculture'
}
$allow = @('!','Z-','Z+','00:00')
$entries = [ordered]@{}
function Slug([string]$value) {
 $slug = ($value.ToLowerInvariant() -replace '[^a-z0-9]+','_').Trim('_')
 if($slug.Length -gt 48){$slug=$slug.Substring(0,48).TrimEnd('_')}
 if(!$slug){$slug='label'}
 return $slug
}
foreach($relative in $files.Keys) {
 $path = Join-Path $Root $relative
 $text = [IO.File]::ReadAllText($path,[Text.UTF8Encoding]::new($false,$true))
 $namespace = $files[$relative]
 $seen = @{}
 $pattern = '<(?<tag>title|div|h[1-6]|button|small|span|p|strong)(?<attrs>[^>]*)>(?<value>[^<>]*\S[^<>]*)</\k<tag>>'
 $text = [regex]::Replace($text,$pattern,{param($m)
   $attrs=$m.Groups['attrs'].Value;$value=$m.Groups['value'].Value
   if($attrs -match '\bdata-l10n=' -or $allow -contains $value){return $m.Value}
   $id = if($attrs -match '\bid="([^"]+)"'){$Matches[1]}else{Slug $value}
   $base="$namespace.$id";$key=$base
   if($seen.ContainsKey($base)){$seen[$base]++;$key="$base.$($seen[$base])"}else{$seen[$base]=1}
   $entries[$key]=$value
   return "<$($m.Groups['tag'].Value)$attrs data-l10n=`"$key`">$value</$($m.Groups['tag'].Value)>"
 })
 $text = [regex]::Replace($text,'<input(?<attrs>[^>]*)\bplaceholder="(?<value>[^"]+)"(?<tail>[^>]*)>',{param($m)
   if($m.Value -match '\bdata-l10n-placeholder='){return $m.Value}
   $attrs=$m.Groups['attrs'].Value;$value=$m.Groups['value'].Value;$tail=$m.Groups['tail'].Value
   $id = if($attrs -match '\bid="([^"]+)"'){$Matches[1]}else{Slug $value}
   $key="$namespace.$id.placeholder";$entries[$key]=$value
   return "<input$attrs placeholder=`"$value`" data-l10n-placeholder=`"$key`"$tail>"
 })
 [IO.File]::WriteAllText($path,$text,[Text.UTF8Encoding]::new($false))
}
$enPath=Join-Path $Root 'content/rmlui/localization/en.json'
$qpsPath=Join-Path $Root 'content/rmlui/localization/qps-long.json'
$en=[ordered]@{};(Get-Content $enPath -Raw -Encoding utf8|ConvertFrom-Json).psobject.Properties|ForEach-Object{$en[$_.Name]=$_.Value}
$qps=[ordered]@{};(Get-Content $qpsPath -Raw -Encoding utf8|ConvertFrom-Json).psobject.Properties|ForEach-Object{$qps[$_.Name]=$_.Value}
foreach($item in $entries.GetEnumerator()){$en[$item.Key]=$item.Value;$qps[$item.Key]="[[ $($item.Value) -- extended localization fixture ]]"}
[IO.File]::WriteAllText($enPath,($en|ConvertTo-Json -Depth 3),[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($qpsPath,($qps|ConvertTo-Json -Depth 3),[Text.UTF8Encoding]::new($false))
$include=Join-Path $Root 'src/gui/ui/localization/UiTextStaticEntries.inc'
$lines=@(if(Test-Path $include){[IO.File]::ReadAllLines($include,[Text.UTF8Encoding]::new($false))})+@($entries.GetEnumerator()|ForEach-Object {'{"'+($_.Key -replace '\\','\\' -replace '"','\"')+'","'+($_.Value -replace '\\','\\' -replace '"','\"')+'"},'})
[IO.File]::WriteAllLines($include,$lines,[Text.UTF8Encoding]::new($false))
Write-Host "Migrated $($entries.Count) static localization entries."
