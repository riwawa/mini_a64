(()=>{
    let Compiler=null;
    let Assembler=null;
    let CPU=null;

    let variables=[];
    let words=[];
    let previousRegisters=null;
    let previousMemory=null;

    let animationToken=0;
    let animating=false;

    const compileButton=document.getElementById("compile");
    const stepButton=document.getElementById("step");
    const runButton=document.getElementById("run");
    const resetButton=document.getElementById("reset");

    const source=document.getElementById("source");
    const assembly=document.getElementById("assembly");
    const symbols=document.getElementById("symbols");
    const machine=document.getElementById("machine");

    const registers=document.getElementById("registers");
    const memoryTable=document.getElementById("memory");
    const pc=document.getElementById("pc");
    const nzcv=document.getElementById("nzcv");
    const execution=document.getElementById("execution");
    const status=document.getElementById("status");

    function setStatus(message,type=""){
        status.textContent=message;
        status.className="status";

        if(type)
            status.classList.add(type);
    }

    function compilerReady(){
        return new Promise(resolve=>{
            if(typeof Module==="undefined"){
                resolve(null);
                return;
            }

            if(Module.calledRun){
                resolve(Module);
                return;
            }

            const previous=Module.onRuntimeInitialized;

            Module.onRuntimeInitialized=()=>{
                if(previous)
                    previous();

                resolve(Module);
            };
        });
    }

    Promise.all([
        compilerReady(),
        MiniA64Assembler(),
        MiniA64CPU()
    ])
    .then(modules=>{
        Compiler=modules[0];
        Assembler=modules[1];
        CPU=modules[2];

        if(!Compiler || !Assembler || !CPU)
            throw new Error("One or more WASM modules failed to initialize.");

        compileButton.disabled=false;
        setStatus("Ready.","done");
    })
    .catch(error=>{
        console.error(error);
        setStatus(
            `WASM loading failed: ${error.message}`,
            "error"
        );
    });

    function getPC(){
        return CPU.ccall(
            "mini_cpu_pc",
            "bigint",
            [],
            []
        );
    }

    function cpuFinished(){
        return CPU.ccall(
            "mini_cpu_finished",
            "number",
            [],
            []
        )!==0;
    }

    function readRegisters(){
        const values=[];

        for(let i=0;i<31;i++){
            values.push(
                CPU.ccall(
                    "mini_cpu_register",
                    "bigint",
                    ["number"],
                    [i]
                )
            );
        }

        return values;
    }

    function readVariables(){
        return variables.map(variable=>{
            const value=CPU.ccall(
                "mini_cpu_memory64",
                "bigint",
                ["bigint"],
                [BigInt(variable.address)]
            );

            return {
                ...variable,
                value
            };
        });
    }

    function renderRegisters(values){
        registers.innerHTML="";

        for(let i=0;i<values.length;i++){
            const item=document.createElement("div");
            item.className="register";

            if(
                previousRegisters &&
                previousRegisters[i]!==values[i]
            ){
                item.classList.add("changed");
            }

            const name=document.createElement("span");
            name.textContent=`X${i}`;

            const value=document.createElement("strong");
            value.textContent=
                `0x${values[i].toString(16).padStart(16,"0")}`;

            item.appendChild(name);
            item.appendChild(value);

            registers.appendChild(item);
        }
    }

    function renderMemory(values){
        memoryTable.innerHTML="";

        for(let i=0;i<values.length;i++){
            const variable=values[i];
            const row=document.createElement("tr");

            if(
                previousMemory &&
                previousMemory[i]!==variable.value
            ){
                row.classList.add("memory-changed");
            }

            const name=document.createElement("td");
            const address=document.createElement("td");
            const value=document.createElement("td");

            name.textContent=variable.name;
            address.textContent=
                `0x${variable.address.toString(16)}`;
            value.textContent=variable.value.toString();

            row.appendChild(name);
            row.appendChild(address);
            row.appendChild(value);

            memoryTable.appendChild(row);
        }
    }

    function machineIndexFromPC(value){
        if(value<0x1000n)
            return -1;

        return Number((value-0x1000n)/4n);
    }

    function highlightMachine(executedPC=null){
        document
            .querySelectorAll(".machine-line")
            .forEach(line=>{
                line.classList.remove("current");
                line.classList.remove("executed");
            });

        if(executedPC!==null){
            const index=machineIndexFromPC(executedPC);

            const line=document.querySelector(
                `.machine-line[data-index="${index}"]`
            );

            if(line)
                line.classList.add("executed");
        }

        if(cpuFinished())
            return;

        const currentPC=getPC();
        const currentIndex=machineIndexFromPC(currentPC);

        const currentLine=document.querySelector(
            `.machine-line[data-index="${currentIndex}"]`
        );

        if(currentLine){
            currentLine.classList.add("current");

            currentLine.scrollIntoView({
                behavior:"smooth",
                block:"nearest"
            });
        }
    }

    function refreshCPU(executedPC=null){
        const pcValue=getPC();

        pc.textContent=
            `0x${pcValue.toString(16).padStart(16,"0")}`;

        const flags=CPU.ccall(
            "mini_cpu_nzcv",
            "number",
            [],
            []
        );

        nzcv.textContent=
            `${(flags>>3)&1}`+
            `${(flags>>2)&1}`+
            `${(flags>>1)&1}`+
            `${flags&1}`;

        const currentRegisters=readRegisters();
        const currentMemory=readVariables();

        renderRegisters(currentRegisters);
        renderMemory(currentMemory);
        highlightMachine(executedPC);

        previousRegisters=[...currentRegisters];
        previousMemory=currentMemory.map(item=>item.value);

        if(cpuFinished()){
            execution.textContent="Finished";
            stepButton.disabled=true;
            runButton.disabled=true;
            setStatus("Program finished.","done");
        }else{
            execution.textContent="Paused";
            stepButton.disabled=false;
            runButton.disabled=false;
        }
    }

    function renderMachineCode(){
        machine.innerHTML="";

        for(let i=0;i<words.length;i++){
            const word=words[i]>>>0;
            const address=0x1000+i*4;

            const line=document.createElement("div");
            line.className="machine-line";
            line.dataset.index=i;

            const addressElement=document.createElement("span");
            addressElement.className="address";
            addressElement.textContent=
                `0x${address.toString(16).padStart(4,"0")}`;

            const index=document.createElement("span");
            index.className="index";
            index.textContent=i.toString().padStart(2,"0");

            const hex=document.createElement("span");
            hex.className="hex";
            hex.textContent=
                `0x${word.toString(16).padStart(8,"0")}`;

            const binary=document.createElement("span");
            binary.className="binary";
            binary.textContent=
                word.toString(2).padStart(32,"0");

            line.appendChild(addressElement);
            line.appendChild(index);
            line.appendChild(hex);
            line.appendChild(binary);

            machine.appendChild(line);
        }
    }

    function prepareCPU(){
        CPU.ccall(
            "mini_cpu_clear_program",
            null,
            [],
            []
        );

        for(const word of words){
            const ok=CPU.ccall(
                "mini_cpu_add_instruction",
                "number",
                ["number"],
                [word]
            );

            if(!ok)
                return false;
        }

        return CPU.ccall(
            "mini_cpu_prepare",
            "number",
            [],
            []
        )!==0;
    }

    function compileProgram(){
        if(!Compiler || !Assembler || !CPU){
            setStatus(
                "WASM modules are not ready.",
                "error"
            );
            return;
        }

        animationToken++;
        animating=false;

        variables=[];
        words=[];
        previousRegisters=null;
        previousMemory=null;

        symbols.innerHTML="";
        registers.innerHTML="";
        memoryTable.innerHTML="";
        machine.innerHTML="";

        pc.textContent="—";
        nzcv.textContent="—";
        execution.textContent="Not loaded";

        stepButton.disabled=true;
        runButton.disabled=true;
        resetButton.disabled=true;

        setStatus("Compiling...","running");

        const result=Compiler.ccall(
            "mini_compile",
            "number",
            ["string"],
            [source.value]
        );

        if(!result){
            assembly.textContent="Compilation failed.";
            setStatus("Compilation failed.","error");
            return;
        }

        const asm=Compiler.UTF8ToString(result);
        assembly.textContent=asm;

        const symbolCount=Compiler.ccall(
            "mini_symbol_count",
            "number",
            [],
            []
        );

        for(let i=0;i<symbolCount;i++){
            const ptr=Compiler.ccall(
                "mini_symbol_name",
                "number",
                ["number"],
                [i]
            );

            const length=Compiler.ccall(
                "mini_symbol_name_length",
                "number",
                ["number"],
                [i]
            );

            const address=Compiler.ccall(
                "mini_symbol_address",
                "number",
                ["number"],
                [i]
            );

            const name=Compiler.UTF8ToString(ptr,length);

            variables.push({
                name,
                address
            });

            const row=document.createElement("tr");

            const nameCell=document.createElement("td");
            const addressCell=document.createElement("td");

            nameCell.textContent=name;
            addressCell.textContent=
                `0x${address.toString(16)}`;

            row.appendChild(nameCell);
            row.appendChild(addressCell);

            symbols.appendChild(row);
        }

        const assembled=Assembler.ccall(
            "mini_assemble",
            "number",
            ["string"],
            [asm]
        );

        if(!assembled){
            setStatus("Assembly failed.","error");
            return;
        }

        const instructionCount=Assembler.ccall(
            "mini_instruction_count",
            "number",
            [],
            []
        );

        for(let i=0;i<instructionCount;i++){
            words.push(
                Assembler.ccall(
                    "mini_instruction_word",
                    "number",
                    ["number"],
                    [i]
                )>>>0
            );
        }

        renderMachineCode();

        if(!prepareCPU()){
            setStatus(
                "Could not prepare CPU.",
                "error"
            );
            return;
        }

        resetButton.disabled=false;
        execution.textContent="Paused";

        refreshCPU();

        setStatus(
            `${instructionCount} instructions loaded. Ready to step.`,
            "done"
        );
    }

    function stepCPU(){
        if(!CPU || cpuFinished())
            return;

        const beforePC=getPC();

        const ok=CPU.ccall(
            "mini_cpu_step",
            "number",
            [],
            []
        );

        refreshCPU(beforePC);

        if(!ok && !cpuFinished()){
            setStatus(
                "CPU execution error.",
                "error"
            );
        }
    }

    function resetCPU(){
        animationToken++;
        animating=false;

        previousRegisters=null;
        previousMemory=null;

        if(!prepareCPU()){
            setStatus(
                "CPU reset failed.",
                "error"
            );
            return;
        }

        execution.textContent="Paused";

        stepButton.disabled=false;
        runButton.disabled=false;

        refreshCPU();

        setStatus(
            "CPU reset to 0x1000.",
            "done"
        );
    }

    function sleep(ms){
        return new Promise(resolve=>{
            setTimeout(resolve,ms);
        });
    }

    async function runCPU(){
        if(animating || !CPU || cpuFinished())
            return;

        animating=true;

        const token=++animationToken;

        compileButton.disabled=true;
        stepButton.disabled=true;
        runButton.disabled=true;

        execution.textContent="Running";
        setStatus("Executing program...","running");

        while(
            token===animationToken &&
            !cpuFinished()
        ){
            const beforePC=getPC();

            const ok=CPU.ccall(
                "mini_cpu_step",
                "number",
                [],
                []
            );

            refreshCPU(beforePC);

            if(!ok && !cpuFinished()){
                setStatus(
                    "CPU execution error.",
                    "error"
                );
                break;
            }

            await sleep(350);
        }

        if(token!==animationToken)
            return;

        animating=false;

        compileButton.disabled=false;
        resetButton.disabled=false;

        if(!cpuFinished()){
            stepButton.disabled=false;
            runButton.disabled=false;
            execution.textContent="Paused";
        }
    }

    compileButton.addEventListener(
        "click",
        compileProgram
    );

    stepButton.addEventListener(
        "click",
        stepCPU
    );

    runButton.addEventListener(
        "click",
        runCPU
    );

    resetButton.addEventListener(
        "click",
        resetCPU
    );
})();