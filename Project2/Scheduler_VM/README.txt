# Operating Systems (ECE318) - Project 2

## Authors

- NIKAS IOANNIS IASON - 3771 - ioannikas@uth.gr
- PANAGIOTIS NIKOLAOS TSOGKAS - 3672 - ptsogkas@uth.gr
- DIMITRIOS TSIANTOS - 3796 - dtsiantos@uth.gr


## Usage

### Run Simulation and generate plots

To execute the scheduler simulation and generate the related plots based on the output, you can use the `run.sh` helper
script, which executes the `make` command with the appropriate parameters and also the `plot.py` script to generate the
plots. This requires the python packages from the `requirements.txt` file to be installed. To set up the python
dependencies for the first time, run the following commands:

```cmd
python -m venv venv
```

```cmd
# Linux/MacOS
source venv/bin/activate

# Windows (CMD)
venv\Scripts\activate.bat

# Windows (PowerShell)
venv\Scripts\Activate.ps1
```

```cmd
pip install -r requirements.txt
```

Finally, you can run the run.sh script:

```cmd
bash run.sh
```


### Run the Simulation only

You can skip the generation of the plots using the `--no-plot` option:

```cmd
bash run.sh --no-plot
```

### Additional Script Arguments

The `run.sh` script supports several command-line options:

- `--help`  
    Displays a help message with a list of all supported options.

    ```cmd
        bash run.sh --help
    ```

- `--hide=all`  
    Prevents plots from being displayed. The plot images will still be saved to disk if applicable.
    
    ```cmd
        bash run.sh --hide=all
    ```