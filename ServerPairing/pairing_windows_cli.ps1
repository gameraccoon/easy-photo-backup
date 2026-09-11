param (
    [Parameter(Position = 0, Mandatory = $true)]
    [string]$SasCode
)

Add-Type -AssemblyName Microsoft.VisualBasic

$msgText = "Pairing Code: $SasCode`n`nDid the client accept the code?"
$choice = [Microsoft.VisualBasic.Interaction]::MsgBox($msgText, 'YesNo,Question,SystemModal', 'Pairing Confirmation')

if ($choice -eq 'Yes') {
    $clientName = [Microsoft.VisualBasic.Interaction]::InputBox("Give this client a name:", "Client Name", "")
    
    if (-not [string]::IsNullOrWhiteSpace($clientName)) {
        Write-Output $clientName
        exit 0
    }
}

exit 1
