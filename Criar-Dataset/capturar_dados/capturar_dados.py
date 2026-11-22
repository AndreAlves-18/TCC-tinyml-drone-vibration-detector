import argparse
import csv
import os
import time

import serial

BAUD    = 115200          # ignorado pela serial USB da Pico, mas exigido pelo pyserial
FS      = 790.7           # taxa real medida do ADXL345 (nominal: 800 Hz)
ESCALA  = 0.0039 * 9.81   # m/s² por LSB (resolução total, qualquer faixa)
TIMEOUT_DADOS = 3.0       # segundos sem amostras = coleta travada
MAX_LINHAS_XLSX = 1_048_000

CABECALHO = ["Time (s)", "X Acceleration", "Y Acceleration", "Z Acceleration", "Gap"]

parser = argparse.ArgumentParser(description="Captura de dados do ADXL345 pela serial USB")
parser.add_argument("arquivo", nargs="?",
                    help="nome base do arquivo de saída (padrão: dataset_<data_hora>)")
parser.add_argument("-p", "--porta", default="/dev/ttyACM0",
                    help="porta serial (padrão: /dev/ttyACM0)")
parser.add_argument("--sem-xlsx", action="store_true",
                    help="grava só o CSV, sem gerar o xlsx no final")
args = parser.parse_args()

base = args.arquivo or f"dataset_{time.strftime('%Y%m%d_%H%M%S')}"
for ext in (".xlsx", ".csv"):
    if base.endswith(ext):
        base = base[:-len(ext)]
CSV_SAIDA  = base + ".csv"
XLSX_SAIDA = base + ".xlsx"

# evita sobrescrever uma coleta já feita
for caminho in (CSV_SAIDA, XLSX_SAIDA):
    if os.path.exists(caminho):
        print(f"ERRO: o arquivo '{caminho}' já existe. Escolha outro nome.")
        raise SystemExit(1)

print(f"Gravando em: {CSV_SAIDA}")
print("Pressione Ctrl+C UMA vez para parar.\n")

n_amostras   = 0
overruns     = 0      # FIFO do sensor encheu (amostras perdidas no sensor)
saltos       = 0      # contador pulou (linhas perdidas no caminho)
ultimo_n     = None
gap_pendente = False  # marca a próxima amostra como início após uma perda
t_inicio     = None
ultimo_dado  = None
motivo       = "interrompido pelo usuário"

# O CSV é escrito durante a coleta: se algo der errado, o que já chegou fica salvo.
with open(CSV_SAIDA, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(CABECALHO)

    try:
        with serial.Serial(args.porta, BAUD, timeout=1) as ser:
            ser.reset_input_buffer()

            buffer = b""
            while True:
                if ultimo_dado is not None and time.time() - ultimo_dado > TIMEOUT_DADOS:
                    motivo = f"nenhuma amostra recebida por {TIMEOUT_DADOS:.0f} s (coleta travou)"
                    break

                # lê tudo o que já chegou de uma vez (bem mais rápido que readline,
                # que no pyserial lê byte a byte)
                dados = ser.read(ser.in_waiting or 1)
                if not dados:
                    continue
                buffer += dados
                *linhas, buffer = buffer.split(b"\n")

                agora = time.time()
                for bruta in linhas:
                    linha = bruta.decode("utf-8", errors="ignore").strip()
                    if not linha:
                        continue

                    # linhas de status do firmware começam com '#'
                    if linha.startswith("#"):
                        if "OVERRUN" in linha:
                            overruns += 1
                            gap_pendente = True
                        else:
                            print(f"\n{linha}")
                        continue

                    partes = linha.split(",")
                    if len(partes) != 4:
                        continue
                    try:
                        n, x, y, z = (int(v) for v in partes)
                    except ValueError:
                        continue   # cabeçalho ou linha incompleta

                    if ultimo_n is not None and n != ultimo_n + 1:
                        saltos += 1
                        gap_pendente = True
                    ultimo_n = n

                    if t_inicio is None:
                        t_inicio = agora
                    ultimo_dado = agora

                    w.writerow([f"{n_amostras / FS:.6f}",
                                f"{x * ESCALA:.6f}", f"{y * ESCALA:.6f}", f"{z * ESCALA:.6f}",
                                int(gap_pendente)])
                    gap_pendente = False
                    n_amostras += 1

                    # status e gravação em disco 2 vezes por segundo
                    if n_amostras % 400 == 0:
                        f.flush()
                        duracao = agora - t_inicio
                        hz_real = n_amostras / duracao if duracao > 0 else 0.0
                        print(f"\r{hz_real:6.1f} Hz  |  {n_amostras} amostras  |  "
                              f"overruns: {overruns}  |  saltos: {saltos}",
                              end="", flush=True)

    except KeyboardInterrupt:
        pass
    except serial.SerialException as e:
        motivo = f"conexão perdida ({e})"

print(f"\n\nParada: {motivo}")
print(f"CSV salvo: {CSV_SAIDA} com {n_amostras} amostras ({n_amostras / FS:.1f} s)")

if overruns or saltos:
    print(f"ATENCAO: {overruns} overruns e {saltos} saltos. Os pontos de perda "
          f"estão marcados com 1 na coluna Gap.")
else:
    print("Nenhuma amostra perdida.")

if n_amostras == 0 or args.sem_xlsx:
    raise SystemExit

if n_amostras > MAX_LINHAS_XLSX:
    print("Coleta longa demais para o xlsx (limite de ~1 milhão de linhas). Use o CSV.")
    raise SystemExit

# Conversão para xlsx a partir do CSV já salvo
print("\nGerando o xlsx. Isso pode levar alguns minutos; não aperte Ctrl+C...")
try:
    import openpyxl
    wb = openpyxl.Workbook(write_only=True)
    ws = wb.create_sheet("Dataset")
    with open(CSV_SAIDA, newline="") as f:
        r = csv.reader(f)
        ws.append(next(r))
        for row in r:
            ws.append([float(row[0]), float(row[1]), float(row[2]), float(row[3]), int(row[4])])
    wb.save(XLSX_SAIDA)
    print(f"xlsx salvo: {XLSX_SAIDA}")
except KeyboardInterrupt:
    if os.path.exists(XLSX_SAIDA):
        os.remove(XLSX_SAIDA)
    print(f"\nConversão interrompida. Os dados continuam completos em {CSV_SAIDA}.")