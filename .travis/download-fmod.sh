TOKEN=$(curl -X POST -u $1:$2 https://www.fmod.com/api-login | jq -r '.token')
if [[ $3  == "x11" ]]
then
  URL=$(curl -H 'Accept: application/json' -H "Authorization: Bearer ${TOKEN}" https://www.fmod.com/api-get-download-link\?path\=files/fmodstudio/api/Linux/\&filename\=fmodstudioapi20006linux.tar.gz\&user\=$1 | jq -r '.url')
  wget -O fmodstudioapi20006linux.tar.gz $URL
fi
if [[ $3 == "osx" ]]
then
  URL=$(curl -H 'Accept: application/json' -H "Authorization: Bearer ${TOKEN}" https://www.fmod.com/api-get-download-link\?path\=files/fmodstudio/api/Mac/\&filename\=fmodstudioapi20006mac-installer.dmg\&user\=$1 | jq -r '.url')
  wget -O fmodstudioapi20006osx.dmg $URL
fi
if [[ $3 == "windows" ]]
then
  URL=$(curl -H 'Accept: application/json' -H "Authorization: Bearer ${TOKEN}" https://www.fmod.com/api-get-download-link\?path\=files/fmodstudio/api/Win/\&filename\=fmodstudioapi20006win-installer.exe\&user\=$1 | jq -r '.url')
  wget -O fmodstudioapi20006win-installer.exe $URL
fi
if [[ $3 == "iphone" ]]
then
  URL=$(curl -H 'Accept: application/json' -H "Authorization: Bearer ${TOKEN}" https://www.fmod.com/api-get-download-link\?path\=files/fmodstudio/api/iOS/\&filename\=fmodstudioapi20006ios-installer.dmg\&user\=$1 | jq -r '.url')
  wget -O fmodstudioapi20006ios.dmg $URL
fi
if [[ $3 == "android" ]]
then
  URL=$(curl -H 'Accept: application/json' -H "Authorization: Bearer ${TOKEN}" https://www.fmod.com/api-get-download-link\?path\=files/fmodstudio/api/Android/\&filename\=fmodstudioapi20006android.tar.gz\&user\=$1 | jq -r '.url')
  wget -O fmodstudioapi20006android.tar.gz $URL
fi
