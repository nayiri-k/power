import yaml

configs = [
    'TinyRocketConfig',
    'RocketConfig',
    'HwachaRocketConfig',
    'GemminiRocketConfig',
    'SmallBoomConfig',
    'LargeBoomConfig'
]

for config in configs:

    hammer_ir = {}
    hammer_ir['power.inputs.saifs'] = []
    hammer_ir['power.inputs.waveforms'] = []
    hammer_ir['power.inputs.report_configs'] = [
        {
            'waveform_path': f"/tools/C/nayiri/power/chipyard-tstech28/vlsi/output/chipyard.TestHarness.{config}/median.fsdb",
            'module': 'chiptop',
            # 'levels': 4,
            'frame_count': 1000,
            # 'start_time': '0.5us',
            # 'end_time': '3us',
            # 'report_name': 'my_median.rpt',
            'output_formats': [
                'plot_profile'
            ]
        }
    ]

    # print(yaml.dump(hammer_ir, default_flow_style=False, sort_keys=False))
    with open(f"yaml_configs/{config}.yml",'w') as f:
        yaml.dump(hammer_ir, f, default_flow_style=False, sort_keys=False)