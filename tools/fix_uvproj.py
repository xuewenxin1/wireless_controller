# -*- coding: utf-8 -*-
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
origin = (ROOT / "tools" / "uvproj_origin.xml").read_text(encoding="utf-8-sig")


def group_option(bank_no):
    return f"""          <GroupOption>
            <CommonProperty>
              <UseCPPCompiler>0</UseCPPCompiler>
              <RVCTCodeConst>0</RVCTCodeConst>
              <RVCTZI>0</RVCTZI>
              <RVCTOtherData>0</RVCTOtherData>
              <ModuleSelection>0</ModuleSelection>
              <IncludeInBuild>2</IncludeInBuild>
              <AlwaysBuild>2</AlwaysBuild>
              <GenerateAssemblyFile>2</GenerateAssemblyFile>
              <AssembleAssemblyFile>2</AssembleAssemblyFile>
              <PublicsOnly>2</PublicsOnly>
              <StopOnExitCode>11</StopOnExitCode>
              <CustomArgument></CustomArgument>
              <IncludeLibraryModules></IncludeLibraryModules>
              <ComprImg>1</ComprImg>
              <BankNo>{bank_no}</BankNo>
            </CommonProperty>
            <Group51>
              <C51>
                <RegisterColoring>2</RegisterColoring>
                <VariablesInOrder>2</VariablesInOrder>
                <IntegerPromotion>2</IntegerPromotion>
                <uAregs>2</uAregs>
                <UseInterruptVector>2</UseInterruptVector>
                <Fuzzy>8</Fuzzy>
                <Optimize>10</Optimize>
                <WarningLevel>3</WarningLevel>
                <SizeSpeed>2</SizeSpeed>
                <ObjectExtend>2</ObjectExtend>
                <ACallAJmp>2</ACallAJmp>
                <InterruptVectorAddress>0</InterruptVectorAddress>
                <VariousControls>
                  <MiscControls></MiscControls>
                  <Define></Define>
                  <Undefine></Undefine>
                  <IncludePath></IncludePath>
                </VariousControls>
              </C51>
              <Ax51>
                <UseMpl>2</UseMpl>
                <UseStandard>2</UseStandard>
                <UseCase>2</UseCase>
                <UseMod51>2</UseMod51>
                <VariousControls>
                  <MiscControls></MiscControls>
                  <Define></Define>
                  <Undefine></Undefine>
                  <IncludePath></IncludePath>
                </VariousControls>
              </Ax51>
            </Group51>
          </GroupOption>"""


def file_entry(name, ftype=1):
    return f"""            <File>
              <FileName>{name}</FileName>
              <FileType>{ftype}</FileType>
              <FilePath>.\\{name}</FilePath>
            </File>"""


def make_group(name, bank_no, files, banked=True):
    opts = (group_option(bank_no) + "\n") if banked else ""
    files_xml = "\n".join(file_entry(n, t) for n, t in files)
    return f"""        <Group>
          <GroupName>{name}</GroupName>
{opts}          <Files>
{files_xml}
          </Files>
        </Group>"""


groups = "\n".join(
    [
        make_group(
            "startup",
            0,
            [("STARTUP_M5.A51", 2), ("L51_BANK.A51", 2)],
            banked=False,
        ),
        make_group(
            "application",
            0,
            [
                ("main.c", 1),
                ("sys.c", 1),
                ("uart.c", 1),
                ("rtc.c", 1),
                ("Wifipro.c", 1),
                ("app_core.c", 1),
                ("app_core.h", 5),
                ("sync_link.h", 5),
                ("bank_bridge.h", 5),
            ],
        ),
        make_group("bank0_modbus", 1, [("modbus.c", 1)]),
        make_group("bank1_control", 2, [("control.c", 1)]),
        make_group("bank3_protocol", 4, [("protocol.c", 1)]),
        make_group(
            "bank4_wifi",
            5,
            [("mcu_api.c", 1), ("system.c", 1), ("OTA.c", 1), ("wifi.h", 5)],
        ),
        make_group("bank5_upload", 6, [("upload.c", 1), ("ErrorHistory.c", 1)]),
    ]
)

text = origin
start = text.index("<Groups>")
end = text.index("</Groups>") + len("</Groups>")
text = text[:start] + f"<Groups>\n{groups}\n      </Groups>" + text[end:]
text = text.replace("<useCB>0</useCB>", "<useCB>1</useCB>")
text = text.replace("<useL251>0</useL251>", "<useL251>1</useL251>")
text = text.replace("<cBanks>0</cBanks>", "<cBanks>8</cBanks>")
text = re.sub(
    r"(?s)<RCB>.*?</RCB>",
    """              <RCB>
                <Type>0</Type>
                <StartAddress>0x100</StartAddress>
                <Size>0x40ff</Size>
              </RCB>""",
    text,
    count=1,
)
text = re.sub(
    r"(?s)<Ocm1>.*?</Ocm1>",
    """              <Ocm1>
                <Type>0</Type>
                <StartAddress>0x4100</StartAddress>
                <Size>0xbf00</Size>
              </Ocm1>""",
    text,
    count=1,
)
text = text.replace(
    "<UserProg1Name>.\\obj\\1.bat</UserProg1Name>",
    "<UserProg1Name>.\\AfterBuildRun.bat</UserProg1Name>",
)
text = text.replace("<CreateHexFile>1</CreateHexFile>", "<CreateHexFile>0</CreateHexFile>")
text = text.replace("<HexFormatSelection>0</HexFormatSelection>", "<HexFormatSelection>1</HexFormatSelection>")
text = text.replace("<HexSelection>0</HexSelection>", "<HexSelection>1</HexSelection>")

out = ROOT / "T5L51.uvproj"
out.write_bytes(text.replace("\n", "\r\n").encode("utf-8"))
print("OK", out, out.stat().st_size)
