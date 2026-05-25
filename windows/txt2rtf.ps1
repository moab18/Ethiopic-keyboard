param(
    [Parameter(Mandatory=$true)]
    [string]$InputFile,
    [Parameter(Mandatory=$true)]
    [string]$OutputFile
)
$txt = Get-Content $InputFile -Raw
$rtf = '{\rtf1\ansi\deff0 {\fonttbl {\f0 Consolas;}} \f0\fs20 ' +
       $txt.Replace('\', '\\').Replace('{', '\{').Replace('}', '\}').Replace("`r`n", '\par ') +
       '}'
[System.IO.File]::WriteAllText($OutputFile, $rtf)
