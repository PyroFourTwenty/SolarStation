from flask import Flask
from flask_influxdb import InfluxDB
dbname='home'
app = Flask(__name__)
app.config.from_pyfile("config.cfg")
influx_db= InfluxDB(app=app)
#influx_db.database.switch(database=dbname)

def get_measurement_array(datatype, source, value):
    return [
        {
            "measurement":"measurements",
            "fields":{"value":float(value)},
            "tags":{"type":str(datatype), "source": str(source)},
            
        }
    ]

@app.route('/measurement/<solarvoltage>,<solarcurrent>,<batteryvoltage>,<batterycurrent>,<loadvoltage>,<loadcurrent>')
def writeMeasurements(solarvoltage,solarcurrent,batteryvoltage,batterycurrent,loadvoltage,loadcurrent):
    influx_db.database.switch(database="home")

    solar_watts = float(solarvoltage)*float(solarcurrent)
    battery_watts = float(batteryvoltage)*float(batterycurrent)
    load_watts = float(loadvoltage) * float(loadcurrent)

    print(solar_watts, "\t", battery_watts, "\t", load_watts)
    influx_db.write_points(get_measurement_array('voltage', 'solar', solarvoltage))
    influx_db.write_points(get_measurement_array('voltage', 'battery', batteryvoltage))
    influx_db.write_points(get_measurement_array('voltage', 'load', loadvoltage))
    influx_db.write_points(get_measurement_array('current', 'solar', solarcurrent))
    influx_db.write_points(get_measurement_array('current', 'battery', batterycurrent))
    influx_db.write_points(get_measurement_array('current', 'load', loadcurrent))

    influx_db.write_points(get_measurement_array('power', 'solar', solar_watts))
    influx_db.write_points(get_measurement_array('power', 'battery', battery_watts))
    influx_db.write_points(get_measurement_array('power', 'load', load_watts))


    return 'ok'

