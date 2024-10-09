import json
import os
from underground import SubwayFeed
from datetime import datetime, timezone
# define these in preferences.py (I added mine to .gitignore for security)
from preferences import ROUTE, STOP_CODE, RELEVANT_DIRECTION

def minutes_until(dt):
    now = datetime.now(tz=timezone.utc)
    return int((dt - now).total_seconds() / 60.0)

def default(obj):
    if isinstance(obj, datetime):
        return { 'minutes_until_departure': f"{minutes_until(obj)} min" } # TODO: add timezone specification
    raise TypeError('...')

def lambda_handler(event, context):
    
    API_KEY = os.getenv('MTA_API_KEY')   
    
    stops_dict = SubwayFeed.get(ROUTE, api_key=API_KEY).extract_stop_dict()
    my_stop_departures = stops_dict[ROUTE][STOP_CODE + RELEVANT_DIRECTION]
    response = json.dumps(my_stop_departures, default=default)

    return {
        'statusCode': 200,
        'body': response,
    }

