# stonks
Created for 6.08: Embedded Systems at MIT.

# Video

Demo: <a href="https://youtu.be/eVYllzubHmw" target="_blank">https://youtu.be/eVYllzubHmw</a>

# Specifications

Below is how I addressed each specification:

## User Prompt

**Requirement:**

* The ESP32 must prompt the user for a stock to track. You have two options for input here:
    * Have a preset selection of ten Stock ticker symbols such as "GME", "TSLA", "CSX", "APPL", (up to you) from which the user can select from
    * Allow open-ended specification of any stock ticker. In Week 5, we will involve a tilt-based text input for querying Wikipedia, so you're allowed to use and adapt this after you've developed it. You're also welcome to utilize a text input of your own creation using any and all buttons and IMU in the kit.

**Implementation:**

My implementation allows open-ended specification of any stock ticker and uses a tilt-based text input to build each ticker. A long press initializes the user prompt.
When the user orients the device above a predefined angle threshold, the screen will update with the current character. A short press adds each character to the query. An additional long press sets the ticker value to the current query.

## Periodic Requests

**Requirement:**

* Once a stock is specified, the ESP32 must make periodic requests to an online resource every ~ten seconds getting that stocks current price. This stock price should be compared to the previous value of the stock price and do the following:
    * Display the current ticker and price on the LCD
    * And then set the RGB LED to the following colors:
        * Red if the price has dropped
        * Green or blue if the price has increased
        * Yellow or purple if the price has stayed the same

**Implementation:**

To make an HTTP request, I create a client object and connect to the host. I formulate the request into a char array, which is sent to the host of interest. The host parses the request and generates a response, which is placed in the response buffer char array.

To retrieve the stock ticker's current price, I create a Python script on `608dev-2.net` to act as a proxy between my ESP32 and Yahoo's stock ticker page. Every 10 seconds (assuming a ticker is built from the query in the user prompt), the script makes requests to this online resource and parses the HTML using 
`BeautifulSoup` to extract the stock price. The stock price is specifically located in the first `span` on the page with the `data-reactid` attribute that has a value of "50". The script then inputs the stock ticker and price values into a database using the `sqlite3` module. This database enables us to track previous prices from the same ticker, so that we can observe how price changes.
The Python script compares the most recent price to the current stock price and returns a tuple containing the color change required and the current price of the stock.

On the ESP32, I make a POST request to this proxy and parse the response buffer using the `strtok` function to get the stock ticker's current price and color change. I then print the ticker name and current price to the LCD screen.

The color change is represented by a character: 'Y', 'G', or 'R'. If the stock ticker has never been submitted to the database or the stock price has not changed since the previous value, the Python script returns 'Y', and the pins corresponding to
the red and green LEDs are set `HIGH`. If the stock price increased, the Python script returns 'G', and the pin corresponding to the green LED is set `HIGH`. If the stock price decreased, the Python script returns 'R', and the pin corresponding to the red LED
is set `HIGH`.

If the device detects a long press, the current query and ticker are cleared to enable the user to switch to another stock ticker and track price changes. If no long press is detected, the device continues to make requests to the proxy script on the server every 10 seconds, updating the LCD screen and LED display.
