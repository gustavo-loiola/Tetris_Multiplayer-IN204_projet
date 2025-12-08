Perfeito ✔
Aqui está um arquivo **Markdown completo e bem formatado** para vocês colocarem no repositório em:

```
docs/SETUP_SDL2_vcpkg.md
```

---

# 🧩 Setup SDL2 com vcpkg (Windows) — Multiplayer Tetris IN204

Este guia explica como **rodar o projeto Tetris com SDL2 via CMake** usando **vcpkg** no Windows.
Siga estes passos apenas uma vez por máquina.

Requisitos:

* Windows 10/11
* Visual Studio Build Tools 2022 (ou Visual Studio 2022)
* Git instalado
* PowerShell

---

## 1️⃣ Instalar vcpkg (se ainda não tiver)

```powershell
cd C:\Users\SEU_USUARIO
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

> Trocar `SEU_USUARIO` pelo nome correto do usuário Windows.

---

## 2️⃣ Configurar variável de ambiente `VCPKG_ROOT`

```powershell
[System.Environment]::SetEnvironmentVariable(
  "VCPKG_ROOT",
  "C:\Users\SEU_USUARIO\vcpkg",
  "User"
)
```

Depois disso → **fechar e reabrir o PowerShell**.

Verificar se está setado:

```powershell
echo $env:VCPKG_ROOT
```

---

## 3️⃣ Instalar SDL2 via vcpkg

```powershell
cd $env:VCPKG_ROOT
.\vcpkg.exe install sdl2:x64-windows

# Verificar instalação
.\vcpkg.exe list
```

Você deve ver uma linha:

```
sdl2:x64-windows   ...
```

---

## 4️⃣ Clonar o projeto Multiplayer Tetris

```powershell
cd C:\ENSTA\P1\genie_logiciel_poo
git clone <URL_DO_REPO> IN204_projet
cd IN204_projet
```

> Ou `git pull` se já tiver o repo.

---

## 5️⃣ Configurar build com CMake

```powershell
mkdir build
cd build

cmake .. -DBUILD_SDL_GUI=ON -DBUILD_TESTS=ON
```

Se não aparecer erro de SDL → OK!

---

## 6️⃣ Compilar o jogo (executável SDL)

```powershell
cmake --build . --config Debug --target tetris_sdl
```

O resultado deve ser criado em:

```
build/Debug/tetris_sdl.exe
```

---

## 7️⃣ Garantir que `SDL2.dll` está presente no Debug/

O Windows precisa encontrar `SDL2.dll` na mesma pasta do `.exe`.

```powershell
$env:VCPKG_ROOT = "C:\Users\SEU_USUARIO\vcpkg"

copy "$env:VCPKG_ROOT\installed\x64-windows\bin\SDL2.dll" `
     "C:\ENSTA\P1\genie_logiciel_poo\IN204_projet\build\Debug\SDL2.dll" `
     -Force
```

---

## 8️⃣ Rodar o jogo 🎮

```powershell
cd C:\ENSTA\P1\genie_logiciel_poo\IN204_projet\build\Debug
.\tetris_sdl.exe
```

Deve abrir a janela **“Tetris SDL”** com o menu inicial.

---

## 💡 Dicas úteis

* Sempre rodar **a partir do terminal** para ver mensagens de erro SDL.
* Se mexer no `CMakeLists.txt`, execute `cmake ..` novamente.
* Se quiser rodar versão **Release**:

```powershell
cmake --build . --config Release --target tetris_sdl
```

---

## ❗ Problemas comuns

| Sintoma                                        | Causa                                     | Solução                              |
| ---------------------------------------------- | ----------------------------------------- | ------------------------------------ |
| `tetris_sdl.exe` não abre nada                 | `SDL2.dll` não encontrada                 | Copie a DLL para `build/Debug/`      |
| Erro `Could not find SDL2Config.cmake`         | `VCPKG_ROOT` errado ou SDL2 não instalado | Reexecutar passo 2 e 3               |
| Red underline em `#include <SDL.h>` no VS Code | Somente IntelliSense, build OK            | Adicionar includePath (extra abaixo) |

---

### 🔧 Extra — Fixar IntelliSense no VS Code

Adicione esta linha ao arquivo:
`.vscode/c_cpp_properties.json`

```json
"C:/Users/SEU_USUARIO/vcpkg/installed/x64-windows/include/SDL2"
```

Depois **Ctrl+Shift+P** → `Developer: Reload Window`.

---

## 🎯 Conclusão

Depois de seguir este guia, seu ambiente está pronto para:

✔ Build do game single-player SDL
✔ Futuras telas para Multiplayer
✔ Integração com networking
